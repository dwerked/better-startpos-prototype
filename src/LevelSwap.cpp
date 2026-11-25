#include "LevelSwap.hpp"
#include "LevelRef.hpp"
#include <Geode/ui/LoadingSpinner.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/PlayLayer.hpp>

using namespace geode::prelude;

// How a level gets built without freezing the screen. This copies what GD's
// own level page does when you press Play (LevelInfoLayer::playStep3 and
// loadLevelStep in GD's code):
//   1. PlayLayer::scene(level, true) - the `true` means "don't build the
//      objects yet", so creating it is quick.
//   2. Every frame, call processCreateObjectsFromSetup(). It builds a chunk of
//      objects and raises m_loadingProgress toward 1.0.
//   3. When m_loadingProgress reaches 1.0, show the finished scene.
//
// Two ways to use it:
//   Loading screen (default): quit the old level first, show a black screen
//     with a loading circle, build the new level, then fade in.
//   Seamless (setting): freeze the old level on screen with a small loading
//     circle, build the new level behind it, then cut straight to it. Both
//     levels exist for a moment, which GD never does itself, so it's off by
//     default.

// What the next level to load should start at (set by a swap, read once).
static std::optional<std::pair<LevelRef, SwapStart>> s_pendingStart;

// Set just before we quit a level for a swap: GD's quit code ends by calling
// GameManager::returnToLastScene, which would show the level page. We show
// our loading screen there instead (see the GameManager hook below).
static bool s_showLoadingInsteadOfMenu = false;
// Seamless mode: the new level is already built, so skip the menu entirely.
static bool s_skipMenuEntirely = false;
// True while the old level runs GD's quit code for a swap. GD's quit ends by
// fading in the menu music (GameManager::fadeInMenuMusic); during a swap we're
// going straight into another level, so that music is skipped. In seamless mode
// it would otherwise play over the new level.
static bool s_quittingForSwap = false;
// The level whose music must be restarted when it starts (seamless mode only).
// Only compared, never dereferenced, so it can't dangle into a crash.
static PlayLayer* s_restartMusicFor = nullptr;

bool takeMusicRestart(PlayLayer* level) {
    if (!level || level != s_restartMusicFor) return false;
    s_restartMusicFor = nullptr;
    return true;
}

CCScene* makeLoadingScene();

// GD's "current level" pointers (GameManager::m_playLayer / m_gameLayer).
static void pointGameAt(PlayLayer* level) {
    auto gm = GameManager::get();
    gm->m_playLayer = level;
    gm->m_gameLayer = level;
}

// Prepared startposes (saved states) belong to one loaded copy of a level, so
// they'd be lost if the Practice level were closed when you swap to the Target.
// Instead we "park" it: the Practice level's scene stays loaded, off screen,
// while you play the Target. Swapping back shows that same scene again, so
// nothing is rebuilt or re-prepared. Off screen, cocos2d pauses the level's
// timers automatically, and GD's own enter/exit code for a level only flips a
// flag (checked in GD's code), so showing it again is safe.
// Any normal quit (to the menu) throws the parked level away.
static Ref<CCScene> s_parkedScene;
static PlayLayer* s_parkedLevel = nullptr; // kept alive by s_parkedScene
// After a parked level is shown again: where to start it (read once).
static std::optional<std::pair<PlayLayer*, SwapStart>> s_resume;

static void releaseParked() {
    s_parkedLevel = nullptr;
    s_parkedScene = nullptr; // destroys the parked level, unless it's on screen again
}

void dropParkedLevel() {
    if (isSwapInProgress()) return; // a swap may be using it right now
    releaseParked();
}

std::optional<SwapStart> takeParkedResume(PlayLayer* level) {
    if (!s_resume || s_resume->first != level) return std::nullopt;
    auto start = s_resume->second;
    s_resume.reset();
    return start;
}

class $modify(BSSwapPlayLayer, PlayLayer) {
    void onQuit() {
        PlayLayer::onQuit();
        // Leaving for the menu: the parked level won't be needed. Next frame,
        // so it isn't destroyed in the middle of GD's quit code.
        if (!s_quittingForSwap && s_parkedLevel) {
            Loader::get()->queueInMainThread([] { releaseParked(); });
        }
    }
};

class $modify(BSGameManager, GameManager) {
    void returnToLastScene(GJGameLevel* level) {
        if (s_skipMenuEntirely) return;
        if (s_showLoadingInsteadOfMenu) {
            s_showLoadingInsteadOfMenu = false;
            CCDirector::get()->replaceScene(CCTransitionFade::create(0.2f, makeLoadingScene()));
            return;
        }
        GameManager::returnToLastScene(level);
    }

    void fadeInMenuMusic() {
        if (s_quittingForSwap) return;
        GameManager::fadeInMenuMusic();
    }
};

class SwapWaiter : public CCObject {
public:
    Ref<GJGameLevel> m_level;
    Ref<PlayLayer> m_oldLevel;   // seamless mode only: the frozen level on screen
    Ref<CCScene> m_scene;        // the new level's scene, once we start building it
    PlayLayer* m_newLevel = nullptr; // the PlayLayer inside m_scene (m_scene keeps it alive)
    bool m_seamless = false;
    bool m_park = false;         // keep the old level loaded instead of quitting it
    float m_waited = 0.f;

    static constexpr float GIVE_UP_AFTER = 30.f; // seconds

    void tick(float dt) {
        m_waited += dt;
        if (m_waited > GIVE_UP_AFTER) {
            log::warn("Level swap took too long; showing whatever we have");
            if (m_scene) this->show();
            else Notification::create("Couldn't switch levels.\nOpen it from its level page.", NotificationIcon::Error)->show();
            this->stop();
            return;
        }

        // Loading-screen mode: wait until the old level is fully gone and the
        // fade to black has finished before building anything.
        if (!m_scene) {
            bool levelStillOpen = PlayLayer::get() != nullptr;
            bool inTransition = typeinfo_cast<CCTransitionScene*>(CCDirector::get()->getRunningScene()) != nullptr;
            if (levelStillOpen || inTransition) return;
            this->startBuilding();
            return; // build the first chunk next frame
        }

        if (!m_newLevel) {
            log::error("Level swap: new scene has no PlayLayer");
            this->stop();
            return;
        }
        // GD's build step expects to be the "current level", so point GD at the
        // new level just for this call, then back at the one on screen.
        pointGameAt(m_newLevel);
        m_newLevel->processCreateObjectsFromSetup();
        if (m_newLevel->m_loadingProgress < 1.f) {
            if (m_seamless) pointGameAt(m_oldLevel);
            return;
        }

        this->show();
        this->stop();
    }

    void startBuilding() {
        // Seamless mode: mods that draw every frame (Mega Hack) must see the old
        // level while it's on screen, and mods that finish setup a frame later
        // (Custom Keybinds) must see the new one, or its jump listeners die with
        // the old level. Queued tasks run in order before drawing, so queue "point at
        // new" before creating the level and "point at old" after, and point at old now.
        auto newLevelSlot = std::make_shared<PlayLayer*>(nullptr);
        if (m_seamless) {
            Loader::get()->queueInMainThread([newLevelSlot] {
                if (isSwapInProgress() && *newLevelSlot) pointGameAt(*newLevelSlot);
            });
        }

        GameManager::get()->m_sceneEnum = 3; // what GD's level pages set before Play
        // true = "don't create objects yet": we build them a few per frame
        // in tick(), like GD's level pages do.
        m_scene = PlayLayer::scene(m_level, true);
        m_newLevel = m_scene->getChildByType<PlayLayer>(0);
        *newLevelSlot = m_newLevel; // m_scene keeps it alive until the swap ends

        if (m_seamless) {
            pointGameAt(m_oldLevel);
            Loader::get()->queueInMainThread([oldLevel = m_oldLevel] {
                if (isSwapInProgress()) pointGameAt(oldLevel);
            });
        }
    }

    void show() {
        auto gm = GameManager::get();

        if (m_seamless && m_oldLevel && m_park) {
            // Keep the old (Practice) level loaded, off screen, for the swap back.
            if (auto spinner = m_oldLevel->getChildByID("swap-spinner"_spr)) spinner->removeFromParent();
            releaseParked();
            s_parkedScene = typeinfo_cast<CCScene*>(m_oldLevel->getParent());
            s_parkedLevel = s_parkedScene ? m_oldLevel.data() : nullptr;
            pointGameAt(m_newLevel);
            gm->m_sceneEnum = 3;
            s_restartMusicFor = m_newLevel; // the old level's song may still be playing
        }
        else if (m_seamless && m_oldLevel) {
            // Let the old level quit its normal way (saves attempts, stops music)
            // while it's still GD's current level, then hand over to the new one.
            // returnToLastScene is skipped by our hook; the scene change below
            // shows the new level instead.
            pointGameAt(m_oldLevel);
            s_showLoadingInsteadOfMenu = false;
            s_skipMenuEntirely = true;
            s_quittingForSwap = true;
            m_oldLevel->onQuit();
            s_quittingForSwap = false;
            s_skipMenuEntirely = false;
            pointGameAt(m_newLevel);
            gm->m_sceneEnum = 3;
            s_restartMusicFor = m_newLevel; // onQuit just stopped its song
        }

        gm->m_loadingLevel = false; // GD clears this right before showing a level
        auto director = CCDirector::get();
        if (m_seamless) {
            // Straight cut, no fade: this replaces the scene onQuit asked for.
            director->replaceScene(m_scene);
            if (m_park) {
                // replaceScene normally "cleans up" the outgoing scene, which
                // permanently removes every timer in it; a parked level would come
                // back frozen. Parking keeps the scene, so skip that cleanup.
                director->m_bSendCleanupToScene = false;
            }
        }
        else {
            director->replaceScene(CCTransitionFade::create(0.2f, m_scene));
        }
    }

    void stop();
};

static Ref<SwapWaiter> s_waiter;

void SwapWaiter::stop() {
    CCDirector::get()->getScheduler()->unscheduleSelector(schedule_selector(SwapWaiter::tick), this);
    s_waiter = nullptr; // the scheduler still holds a reference until it's done with us
}

CCScene* makeLoadingScene() {
    auto scene = CCScene::create();
    scene->addChild(CCLayerColor::create({ 0, 0, 0, 255 }));
    auto spinner = LoadingSpinner::create(40.f);
    spinner->setPosition(CCDirector::get()->getWinSize() / 2);
    scene->addChild(spinner);
    return scene;
}

bool isSwapInProgress() {
    return s_waiter != nullptr;
}

// Swap back to the parked level: quit the level we're on (without the menu),
// then show the parked scene again. No building needed.
static void returnToParked(PlayLayer* current, SwapStart start) {
    Ref<CCScene> scene = s_parkedScene;
    PlayLayer* parked = s_parkedLevel;
    releaseParked(); // the director keeps the scene alive once it's shown

    s_skipMenuEntirely = true;
    s_quittingForSwap = true;
    current->onQuit();
    s_quittingForSwap = false;
    s_skipMenuEntirely = false;

    pointGameAt(parked);
    GameManager::get()->m_sceneEnum = 3;
    GameManager::get()->m_loadingLevel = false;
    s_resume = { parked, start };
    CCDirector::get()->replaceScene(scene);
}

void swapToLevel(PlayLayer* current, GJGameLevel* level, SwapStart start, bool parkCurrent) {
    if (s_waiter || !current || !level) return;

    bool seamless = Mod::get()->getSettingValue<bool>("seamless-swap");
    bool keepLoaded = seamless && Mod::get()->getSettingValue<bool>("keep-practice-loaded");

    // Going back to the level we parked? Show it again instead of rebuilding.
    if (keepLoaded && s_parkedLevel && s_parkedLevel->m_level
        && LevelRef::fromLevel(s_parkedLevel->m_level).sameLevel(LevelRef::fromLevel(level))) {
        returnToParked(current, start);
        return;
    }
    releaseParked(); // a parked level we're not going back to

    s_pendingStart = { LevelRef::fromLevel(level), start };

    s_waiter = new SwapWaiter();
    s_waiter->release(); // Ref took its own reference; drop the one from `new`
    s_waiter->m_level = level;
    s_waiter->m_seamless = seamless;
    s_waiter->m_park = keepLoaded && parkCurrent;

    if (s_waiter->m_seamless) {
        // Freeze the current level where it is and show a small loading circle
        // in the corner, then start building the new level behind it.
        s_waiter->m_oldLevel = current;
        current->pauseSchedulerAndActions();
        auto spinner = LoadingSpinner::create(25.f);
        auto winSize = CCDirector::get()->getWinSize();
        spinner->setPosition({ winSize.width - 25.f, 25.f });
        spinner->setID("swap-spinner"_spr);
        current->addChild(spinner, 1000);
        s_waiter->startBuilding();
    }
    else {
        // Quit the level GD's normal way, but show our loading screen instead
        // of the level page GD wants next (replacing it in the same frame means
        // GD's choice never appears).
        s_showLoadingInsteadOfMenu = true;
        s_quittingForSwap = true;
        current->onQuit();
        s_quittingForSwap = false;
        s_showLoadingInsteadOfMenu = false; // in case GD didn't call returnToLastScene
    }

    CCDirector::get()->getScheduler()->scheduleSelector(
        schedule_selector(SwapWaiter::tick), s_waiter.data(), 0.f, false
    );
}

SwapStart takePendingStart(GJGameLevel* level) {
    if (!s_pendingStart || !level) return SwapStart::fromZero();
    auto [ref, start] = *s_pendingStart;
    s_pendingStart.reset();
    return ref.sameLevel(LevelRef::fromLevel(level)) ? start : SwapStart::fromZero();
}
