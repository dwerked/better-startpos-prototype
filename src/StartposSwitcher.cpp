// Startpos switcher.
//
// While GD builds the level, PlayLayer::addObject collects every enabled start
// position (object ID 31). Once built, we sort them left to right and find the
// one GD picked (the last enabled one). A switch key makes GD use another
// startpos, or none (start from 0), and restarts the attempt.
//
// "index" is 0 for the start of the level and 1..N for startPositions[0..N-1].
//
// Linked levels: on the Practice level, stop 0 swaps to the Target. On the
// Target, Right swaps to the Practice level's first startpos and Left to its last.
//
// Fast switching: a restart replays every trigger up to the startpos, which is
// slow on big levels. Key presses only move the counter; the one restart happens
// SETTLE_DELAY seconds after the last press.
//
// Go-to key: G freezes the level and turns the display number into a blinking
// cursor. Enter (or G again) jumps to the typed startpos; Esc cancels.

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <Geode/loader/SettingV3.hpp>
#include "Links.hpp"
#include "LevelSwap.hpp"
#include "LevelDownloader.hpp"
#include "FastSP.hpp"
#include "StartDefaults.hpp"
#include "SmartStartpos.hpp"
#include "LearnedGravity.hpp"
#include "ui/Indicator.hpp"

using namespace geode::prelude;

// The level you're typing a go-to number in, or nullptr. While set, that
// level is frozen (see BSFreeze below) and the keyboard goes to the number.
static PlayLayer* s_typingLayer = nullptr;
// Esc may reach GD twice (as a key and as "back"). After Esc cancels typing,
// ignore the pause it would also cause, until the next frame.
static bool s_ignoreNextPause = false;

// Seconds to wait after the last press before restarting. On levels where a
// restart is slow, wait longer: once a restart starts, GD is frozen until it's
// done, and presses made meanwhile can only be handled after it.
static constexpr float SETTLE_DELAY = 0.2f;
static constexpr float SETTLE_DELAY_SLOW = 0.5f;
static constexpr int SLOW_RESTART_MS = 500;

class $modify(BSPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<Ref<StartPosObject>> startPositions;
        std::vector<Ref<GameObject>> portals; // for smart startpos (SmartStartpos.cpp)
        // Each startpos's own settings from the level, before smart startpos
        // changed them, so turning smart startpos off can put them back.
        std::unordered_map<StartPosObject*, PlayerState> ownSettings;
        // Startposes where a gravity orb/pad before them could have flipped
        // you, so their gravity is learned from your runs ("Learned gravity").
        std::unordered_set<StartPosObject*> gravityUnsure;
        // Startposes placed with GD's pause-menu tool already hold your real
        // state, so smart startpos leaves them alone.
        std::unordered_set<StartPosObject*> placedStartposes;
        StartDefault applied;        // the Settings this level is using right now
        bool trackingCrossings = false; // lastPlayerX is valid (set after each restart)
        float lastPlayerX = 0.f;
        int index = 0;
        StartposIndicator* indicator = nullptr; // the "< 4/21 >" display
        bool typing = false;      // go-to key: typing a number right now
        std::string typed;
        std::optional<Link> link; // set if this level is in a link
        bool isPractice = false;
        bool isTarget = false;
        bool pending = false;     // presses waiting for the settle delay
        int targetPresses = 0;    // Target level: net Right(+)/Left(-) presses so far
        int lastRestartMs = 0;    // how long the last restart on this level took
        // Fast SP (preparing every startpos before the level starts).
        bool fastSPChecked = false; // asked takeFastSPRequest already
        bool fastSPUsed = false;    // this level was prepared: switch with no wait
        bool preparing = false;
        bool cancelPrepare = false;
        int prepareNext = 1;           // next startpos index to prepare
        int prepareReturnTo = 0;       // where the level should start afterwards
        CCNode* prepareOverlay = nullptr;
        CCLabelBMFont* prepareLabel = nullptr;
        CCLayerColor* prepareBarFill = nullptr;
    };

    void addObject(GameObject* object) {
        PlayLayer::addObject(object);

        if (isSmartPortal(object)) {
            m_fields->portals.push_back(object);
            return;
        }
        if (object->m_objectID != 31) return;
        auto startPos = static_cast<StartPosObject*>(object);
        // Respect the "Disable" checkbox on the startpos, exactly like vanilla.
        if (startPos->m_startSettings && startPos->m_startSettings->m_disableStartPos) return;

        m_fields->startPositions.push_back(startPos);
    }

    void createObjectsFromSetupFinished() {
        PlayLayer::createObjectsFromSetupFinished();

        auto& list = m_fields->startPositions;
        std::stable_sort(list.begin(), list.end(), [](auto const& a, auto const& b) {
            return a->getPositionX() < b->getPositionX();
        });

        auto fields = m_fields.self();
        this->refreshLinkRole();

        // Smart startpos: fix each startpos's speed, gamemode, etc. to match
        // the portals before it. Done before GD places the player, so even
        // the startpos you begin at is already fixed.
        fields->applied = effectiveDefault(m_level);
        this->applySmart();

        // If we got here by swapping from the Target, begin at the startpos the
        // swap asked for. This runs before GD places the player, so no extra reset.
        auto start = takePendingStart(m_level);
        if (start.from != SwapStart::From::Zero && !list.empty()) {
            int last = static_cast<int>(list.size()) - 1;
            int i = start.from == SwapStart::From::First ? start.offset : last - start.offset;
            this->setActiveStartPosObject(list[std::clamp(i, 0, last)]);
            m_isTestMode = true;
        }
        else if (!list.empty()) {
            // Otherwise begin at your default startpos (Settings): this level's own
            // setting if it has one, else the all-levels one. "Last" is what GD
            // already picked, so that changes nothing.
            int pick = effectiveDefault(m_level).pickIndex(static_cast<int>(list.size()));
            this->setActiveStartPosObject(pick == 0 ? nullptr : list[pick - 1].data());
            // Starting from 0 is a normal attempt; any startpos is a test run.
            m_isTestMode = pick != 0;
        }

        // Find the startpos GD chose so our counter matches what you see.
        fields->index = this->currentIndexInGame();
        log::debug("level built: {} usable startposes, starting at index {}, linked={}",
            list.size(), fields->index, fields->link.has_value());
        this->showIndexLabel();
    }

    // Leaving the pause menu: a link may have been made or removed in it,
    // so check the level's role again and redraw the display to match.
    // Settings changed in it take effect now, without reloading the level.
    void resume() {
        PlayLayer::resume();
        this->refreshLinkRole();
        this->applySettingsChanges(true);
        this->showIndexLabel();
        // You may have picked a startpos in GD's own startpos list in the
        // pause menu. GD switches as the game resumes, so catch up on the
        // next frame, once that has happened.
        Loader::get()->queueInMainThread([self = Ref(this)] { self->syncIndexFromGame(); });
    }

    // Puts our counter on the startpos GD is actually using (after GD itself
    // switched). Not while our own presses or typing are still pending.
    void syncIndexFromGame() {
        auto fields = m_fields.self();
        if (fields->pending || fields->typing || fields->preparing) return;
        fields->index = this->currentIndexInGame();
        this->showIndexLabel();
    }

    // A startpos you placed while playing (GD's pause-menu tool). It joins
    // our list in left-to-right order, so switching, go-to and Fast SP
    // include it like any other.
    void addLiveStartPosObject(StartPosObject* startpos) {
        PlayLayer::addLiveStartPosObject(startpos);
        if (!startpos) return;
        auto fields = m_fields.self();
        auto& list = fields->startPositions;
        if (std::find(list.begin(), list.end(), startpos) != list.end()) return;
        auto at = std::upper_bound(list.begin(), list.end(), startpos->getPositionX(),
            [](float x, auto const& other) { return x < other->getPositionX(); });
        list.insert(at, startpos);
        fields->placedStartposes.insert(startpos);
        if (startpos->m_startSettings) fields->ownSettings[startpos] = PlayerState::from(startpos->m_startSettings);
        log::info("Placed startpos added: now {} startposes", list.size());
        this->syncIndexFromGame();
    }

    // Sets every startpos's settings: its own from the level, then (if smart
    // startpos is on) the portals' version, then any learned gravity. Running
    // it again after the setting changes redoes or undoes smart startpos.
    void applySmart() {
        auto fields = m_fields.self();
        auto& list = fields->startPositions;

        // The first time we see a startpos, remember its own settings. Then
        // put every startpos back to them (undoing any earlier smart pass).
        for (auto& startpos : list) {
            if (!startpos->m_startSettings) continue;
            auto own = fields->ownSettings.try_emplace(startpos.data(), PlayerState::from(startpos->m_startSettings)).first;
            own->second.writeTo(startpos->m_startSettings);
        }
        fields->gravityUnsure.clear();
        if (!fields->applied.smart || list.empty()) return;

        std::vector<GameObject*> portals(fields->portals.begin(), fields->portals.end());
        std::vector<StartPosObject*> startposes(list.begin(), list.end());
        auto result = applySmartStartpos(m_levelSettings, std::move(portals), startposes);

        int learned = 0;
        for (size_t i = 0; i < list.size(); i++) {
            StartPosObject* startpos = list[i];
            auto settings = startpos->m_startSettings;
            if (!settings) continue;
            // Placed startposes already hold your real state: keep it.
            if (fields->placedStartposes.contains(startpos)) {
                fields->ownSettings[startpos].writeTo(settings);
                continue;
            }
            if (!result.gravityUnsure[i]) continue;
            // Where orbs/pads make gravity unsure: use what you had when you
            // last played past that startpos, or else what the startpos itself
            // says (its creator likely tested it more than our guess).
            fields->gravityUnsure.insert(startpos);
            auto known = getLearnedGravity(m_level, startpos->getPositionX());
            if (known) learned++;
            settings->m_isFlipped = known.value_or(fields->ownSettings[startpos].upsideDown);
        }
        log::info("Smart startpos: {} portals, adjusted {} of {} startposes, {} with learned gravity",
            fields->portals.size(), result.changed, list.size(), learned);
    }

    // Compares the Settings now with the ones this level is using, and applies
    // what changed. jumpToDefault: a changed Default startpos also moves you
    // there (not when a swap already chose where to start).
    void applySettingsChanges(bool jumpToDefault) {
        auto fields = m_fields.self();
        auto now = effectiveDefault(m_level);
        auto before = fields->applied;
        fields->applied = now;
        if (fields->isTarget || fields->startPositions.empty()) return;

        bool smartChanged = now.smart != before.smart;
        bool startChanged = now.mode != before.mode
            || (now.mode == StartDefault::Mode::Number && now.number != before.number);
        if (smartChanged) {
            this->applySmart();
            // Saved states hold the old settings; build new ones as you go.
            this->dropSavedStates();
        }
        if (!jumpToDefault || (!smartChanged && !startChanged)) return;
        // Restart now so the change shows: at the new default startpos, or
        // (smart startpos only) where you already are.
        int count = static_cast<int>(fields->startPositions.size());
        this->switchTo(startChanged ? now.pickIndex(count) : this->currentIndexInGame());
    }

    void dropSavedStates() {
        for (auto& startpos : m_fields->startPositions) this->forgetSavedState(startpos);
        m_fields->fastSPUsed = false; // switches are slow again, so use the settle delay
    }

    // Since 2.209 GD saves the state at a startpos on first play, keyed by its ID in
    // m_checkpointsDict, and later restarts load it (that's why switching back is
    // fast). We read it, fill it (Fast SP), and drop an entry when its settings change.

    bool hasSavedState(StartPosObject* startpos) {
        return startpos && m_checkpointsDict && m_checkpointsDict->objectForKey(startpos->m_uniqueID);
    }

    void forgetSavedState(StartPosObject* startpos) {
        if (startpos && m_checkpointsDict) m_checkpointsDict->removeObjectForKey(startpos->m_uniqueID);
    }

    // Each frame, see which startposes the player just passed. For one whose
    // gravity is unsure, remember whether you were upside down there.

    void resetLevel() {
        PlayLayer::resetLevel();
        // The player was just placed somewhere new: start tracking from there,
        // so the jump to a startpos doesn't count as passing the ones before it.
        m_fields->trackingCrossings = false;
    }

    void postUpdate(float dt) {
        PlayLayer::postUpdate(dt);
        this->checkStartposCrossings();
    }

    void checkStartposCrossings() {
        auto fields = m_fields.self();
        if (!m_player1) return;
        float x = m_player1->getPositionX();
        float lastX = fields->lastPlayerX;
        bool wasTracking = fields->trackingCrossings;
        fields->lastPlayerX = x;
        fields->trackingCrossings = true;

        // Only trust plain runs: no dual (two players, two gravities), no
        // platformer (you can walk back and forth), and no sudden jumps
        // forward (teleport portals), which skip past startposes.
        constexpr float MAX_STEP = 60.f;
        if (!wasTracking || m_player1->m_isDead || m_isPlatformer || m_gameState.m_isDualMode) return;
        if (!fields->applied.smart || x <= lastX || x - lastX > MAX_STEP) return;

        auto& list = fields->startPositions;
        for (size_t i = 0; i < list.size(); i++) {
            float startposX = list[i]->getPositionX();
            if (!fields->gravityUnsure.contains(list[i]) || startposX <= lastX || startposX > x) continue;
            bool upsideDown = m_player1->m_isUpsideDown;
            if (!setLearnedGravity(m_level, startposX, upsideDown)) continue; // already known
            log::info("Learned gravity at startpos {}: {}", i + 1, upsideDown ? "upside down" : "normal");
            if (auto settings = list[i]->m_startSettings) settings->m_isFlipped = upsideDown;
            this->forgetSavedState(list[i]); // it was saved with the old gravity
        }
    }

    // Which stop GD is actually using right now: 0 for the start of the level,
    // or 1..N. Anything not in our list (or no startpos at all) counts as 0.
    int currentIndexInGame() {
        auto& list = m_fields->startPositions;
        for (size_t i = 0; i < list.size(); i++) {
            if (list[i] == m_startPosObject) return static_cast<int>(i) + 1;
        }
        return 0;
    }

    // After a seamless swap, the level's song was stopped by the old level's quit
    // code. GD restarts music on every death, which is why it came back then;
    // do the same once, as soon as the level starts.
    // Shown again after being parked (we swapped back from the Target): go to
    // the startpos the swap asked for. Its saved states are still here, so
    // this is fast, and it restarts the music.
    void onEnterTransitionDidFinish() {
        PlayLayer::onEnterTransitionDidFinish();
        auto start = takeParkedResume(this);
        if (!start) return;
        auto fields = m_fields.self();
        this->refreshLinkRole();
        // Settings changed while it was parked apply now (the swap below
        // already says where to start, so no jump to the default startpos).
        this->applySettingsChanges(false);
        int count = static_cast<int>(fields->startPositions.size());
        int index = this->currentIndexInGame();
        if (count > 0 && start->from == SwapStart::From::First) index = std::clamp(start->offset + 1, 1, count);
        if (count > 0 && start->from == SwapStart::From::Last) index = std::clamp(count - start->offset, 1, count);
        this->switchTo(index);
    }

    void startGame() {
        auto fields = m_fields.self();
        // Fast SP: prepare every startpos first, then start for real.
        if (!fields->fastSPChecked) {
            fields->fastSPChecked = true;
            if (takeFastSPRequest(m_level) && fields->startPositions.size() > 1) {
                this->beginPrepare();
                return; // finishPrepare calls startGame again
            }
        }
        PlayLayer::startGame();
        if (takeMusicRestart(this)) this->startMusic();
    }

    // No pausing mid-swap (including GD's auto-pause when the window loses
    // focus): the pause menu would attach to the wrong level.
    // While preparing, Esc (which pauses) skips the rest of the preparing.
    // While typing a go-to number, Esc cancels the typing instead of pausing.
    // Losing window focus cancels it too, then pauses as usual.
    void pauseGame(bool unfocused) {
        if (isSwapInProgress()) return;
        if (m_fields->typing) {
            this->cancelTyping();
            if (!unfocused) return;
        }
        if (s_ignoreNextPause && !unfocused) return;
        if (m_fields->preparing) {
            if (!unfocused) m_fields->cancelPrepare = true;
            return;
        }
        PlayLayer::pauseGame(unfocused);
    }

    // Preparing restarts the level once per startpos. Don't count those as
    // attempts in your stats.
    void updateAttempts() {
        if (m_fields->preparing) return;
        PlayLayer::updateAttempts();
    }

    // Before the level starts, visit every startpos once (one per frame, so the
    // progress bar can update between them) and keep each one's saved state.
    // Each visit is as slow as a normal first switch; afterwards every switch
    // uses a saved state and is near-instant.

    void beginPrepare() {
        auto fields = m_fields.self();
        fields->preparing = true;
        fields->fastSPUsed = true;
        fields->cancelPrepare = false;
        fields->prepareNext = 1;
        fields->prepareReturnTo = this->currentIndexInGame();
        this->createPrepareOverlay();
        this->updatePrepareOverlay(0);
        // Wait one frame so the overlay is on screen before the first freeze.
        this->schedule(schedule_selector(BSPlayLayer::prepareStep), 0.f);
    }

    void prepareStep(float) {
        auto fields = m_fields.self();
        int total = static_cast<int>(fields->startPositions.size());
        if (fields->cancelPrepare || fields->prepareNext > total) {
            this->finishPrepare();
            return;
        }

        int i = fields->prepareNext++;
        StartPosObject* startpos = fields->startPositions[i - 1];
        if (!this->hasSavedState(startpos)) {
            this->setActiveStartPosObject(startpos);
            this->resetLevel();
            // GD saves the state in startMusic; do the same without the music
            // (same calls as startMusic: create it, store it under the ID).
            if (auto saved = this->createCheckpoint()) {
                m_checkpointsDict->setObject(saved, startpos->m_uniqueID);
            }
        }
        this->updatePrepareOverlay(i);
    }

    void finishPrepare() {
        auto fields = m_fields.self();
        this->unschedule(schedule_selector(BSPlayLayer::prepareStep));
        int prepared = 0;
        for (auto& startpos : fields->startPositions) prepared += this->hasSavedState(startpos) ? 1 : 0;
        log::info("Fast SP: prepared {} of {} startposes{}", prepared,
            fields->startPositions.size(), fields->cancelPrepare ? " (skipped the rest)" : "");

        // Go back to where the level would have started (fast now: it's saved).
        int start = fields->prepareReturnTo;
        this->setActiveStartPosObject(start == 0 ? nullptr : fields->startPositions[start - 1].data());
        m_isTestMode = start != 0;
        this->resetLevel();
        fields->index = start;

        if (fields->prepareOverlay) fields->prepareOverlay->removeFromParent();
        fields->prepareOverlay = nullptr;
        fields->prepareLabel = nullptr;
        fields->prepareBarFill = nullptr;
        fields->preparing = false;

        this->startGame(); // fastSPChecked is set, so this starts the level normally
    }

    static constexpr float PREPARE_BAR_WIDTH = 220.f;

    void createPrepareOverlay() {
        auto fields = m_fields.self();
        auto winSize = CCDirector::get()->getWinSize();

        auto overlay = CCLayerColor::create({ 0, 0, 0, 210 });
        overlay->setID("fast-sp-overlay"_spr);

        auto title = CCLabelBMFont::create("Fast SP", "goldFont.fnt");
        title->setScale(0.8f);
        title->setPosition({ winSize.width / 2, winSize.height / 2 + 40.f });
        overlay->addChild(title);

        fields->prepareLabel = CCLabelBMFont::create("", "bigFont.fnt");
        fields->prepareLabel->setScale(0.45f);
        fields->prepareLabel->setPosition({ winSize.width / 2, winSize.height / 2 + 8.f });
        overlay->addChild(fields->prepareLabel);

        auto barBg = CCLayerColor::create({ 60, 60, 60, 255 }, PREPARE_BAR_WIDTH, 8.f);
        barBg->setPosition({ winSize.width / 2 - PREPARE_BAR_WIDTH / 2, winSize.height / 2 - 18.f });
        overlay->addChild(barBg);
        fields->prepareBarFill = CCLayerColor::create({ 90, 230, 90, 255 }, PREPARE_BAR_WIDTH, 8.f);
        fields->prepareBarFill->setPosition(barBg->getPosition());
        fields->prepareBarFill->setScaleX(0.f);
        fields->prepareBarFill->ignoreAnchorPointForPosition(false);
        fields->prepareBarFill->setAnchorPoint({ 0.f, 0.f });
        overlay->addChild(fields->prepareBarFill);

        auto hint = CCLabelBMFont::create("Press Esc to skip", "chatFont.fnt");
        hint->setOpacity(170);
        hint->setPosition({ winSize.width / 2, winSize.height / 2 - 45.f });
        overlay->addChild(hint);

        // On top of everything in the level, including its UI.
        auto parent = this->getParent() ? this->getParent() : static_cast<CCNode*>(this);
        parent->addChild(overlay, 1000);
        fields->prepareOverlay = overlay;
    }

    void updatePrepareOverlay(int done) {
        auto fields = m_fields.self();
        if (!fields->prepareLabel) return;
        int total = static_cast<int>(fields->startPositions.size());
        fields->prepareLabel->setString(fmt::format("Preparing startposes {} / {}", done, total).c_str());
        fields->prepareBarFill->setScaleX(total > 0 ? static_cast<float>(done) / total : 1.f);
    }

    // Is this level linked, and if so, which side is it? Checked on load and
    // again on every switch, so a link made from this level's pause menu
    // works right away without leaving the level.
    void refreshLinkRole() {
        auto fields = m_fields.self();
        fields->link = findLinkFor(m_level);
        fields->isPractice = false;
        fields->isTarget = false;
        if (fields->link) {
            auto self = LevelRef::fromLevel(m_level);
            fields->isPractice = fields->link->practice.sameLevel(self);
            fields->isTarget = !fields->isPractice;
            // Get the other level's data ready now, so swapping doesn't have to wait.
            auto const& other = fields->isPractice ? fields->link->target : fields->link->practice;
            prefetchLevelData(other.resolve());
        }
    }

    // A switch key was pressed: move forward (+1) or back (-1), wrapping around
    // past the ends. This only updates the counter; applyPendingSwitch does the
    // actual restart once the presses stop.
    void switchBy(int delta) {
        if (m_fields->preparing) return;
        this->refreshLinkRole();
        auto fields = m_fields.self();

        if (fields->isTarget) {
            // Count presses: Right x2 means the Practice level's 2nd startpos,
            // Left x2 means its second to last.
            fields->targetPresses += delta;
            if (fields->targetPresses == 0) {
                // Pressed back to where we started: nothing to do.
                fields->pending = false;
                this->unschedule(schedule_selector(BSPlayLayer::applyPendingSwitch));
                this->showIndexLabel();
                return;
            }
            this->showIndexLabel();
            this->scheduleSettle();
            return;
        }

        int count = static_cast<int>(fields->startPositions.size()) + 1; // +1 for stop 0
        // Unlinked level with no usable startposes: nothing to switch to.
        // (A linked Practice level still has the Target as a stop.)
        if (count <= 1 && !fields->isPractice) return;

        // If GD is on a different startpos than we think, something else moved it,
        // usually another switcher (like Mega Hack's) bound to the same keys.
        // Follow the game's real position so we never jump somewhere unexpected.
        // (Skipped while presses are waiting: then our counter is ahead on purpose.)
        if (!fields->pending) {
            int actual = this->currentIndexInGame();
            if (actual != fields->index) {
                warnAboutOtherSwitcher();
                fields->index = actual;
            }
        }

        fields->index = ((fields->index + delta) % count + count) % count;
        this->showIndexLabel();

        // Fast SP prepared this level, so switching is near-instant:
        // no point waiting. Swapping to the Target still waits, because a swap
        // loads a whole level and you may just be cycling past it.
        bool swapping = fields->index == 0 && fields->isPractice;
        if (fields->fastSPUsed && !swapping) {
            this->unschedule(schedule_selector(BSPlayLayer::applyPendingSwitch));
            fields->pending = true;
            this->applyPendingSwitch(0.f);
            return;
        }
        this->scheduleSettle();
    }

    // Jump straight to a startpos (go-to keybind): one restart, no waiting.
    void goTo(int index) {
        auto fields = m_fields.self();
        if (fields->isTarget) return; // the Target has no startposes of its own
        fields->index = std::clamp(index, 0, static_cast<int>(fields->startPositions.size()));
        this->showIndexLabel();
        this->unschedule(schedule_selector(BSPlayLayer::applyPendingSwitch));
        fields->pending = true;
        this->applyPendingSwitch(0.f); // retries shortly if the game hasn't resumed yet
    }

    // (Re)start the settle timer: each new press pushes the restart back.
    void scheduleSettle() {
        m_fields->pending = true;
        this->unschedule(schedule_selector(BSPlayLayer::applyPendingSwitch));
        this->scheduleOnce(schedule_selector(BSPlayLayer::applyPendingSwitch), this->settleDelay());
    }

    float settleDelay() {
        return m_fields->lastRestartMs > SLOW_RESTART_MS ? SETTLE_DELAY_SLOW : SETTLE_DELAY;
    }

    // The presses have stopped: do the one restart (or swap) they add up to.
    void applyPendingSwitch(float) {
        auto fields = m_fields.self();
        if (!fields->pending) return;

        // Can't restart right now (paused, level ending, swap running): try again shortly.
        if (m_isPaused || m_levelEndAnimationStarted || !m_player1 || isSwapInProgress()) {
            this->scheduleOnce(schedule_selector(BSPlayLayer::applyPendingSwitch), SETTLE_DELAY);
            return;
        }
        fields->pending = false;
        this->refreshLinkRole();

        if (fields->isTarget) {
            int n = fields->targetPresses;
            fields->targetPresses = 0;
            if (n != 0) {
                this->swapTo(fields->link->practice,
                    n > 0 ? SwapStart::fromFirst(n - 1) : SwapStart::fromLast(-n - 1));
            }
            return;
        }

        if (fields->index == 0 && fields->isPractice) {
            // If the swap can't start (e.g. still downloading), put the counter
            // back on the startpos we're actually playing.
            if (!this->swapTo(fields->link->target, SwapStart::fromZero())) {
                fields->index = this->currentIndexInGame();
            }
            return;
        }
        this->switchTo(fields->index);
    }

    // Leave this level for the other level in the link. Returns false if the
    // swap couldn't start (the reason is shown as a notification).
    bool swapTo(LevelRef const& ref, SwapStart start) {
        if (m_isPaused || m_levelEndAnimationStarted || !m_player1) return false;

        auto level = ref.resolve();
        if (!level) {
            Notification::create(
                fmt::format("Can't find \"{}\".\nRelink it from the pause menu.", shorten(ref.name)),
                NotificationIcon::Error
            )->show();
            return false;
        }
        // Online levels need their data before they can be played. It normally
        // downloads in the background as soon as you enter a linked level.
        if (!hasLevelData(level)) {
            bool alreadyDownloading = isDownloadingLevel(level);
            prefetchLevelData(level);
            Notification::create(
                alreadyDownloading || isDownloadingLevel(level)
                    ? fmt::format("Still downloading \"{}\".\nTry again in a moment.", shorten(ref.name))
                    : fmt::format("Couldn't download \"{}\".\nCheck your connection.", shorten(ref.name)),
                alreadyDownloading || isDownloadingLevel(level) ? NotificationIcon::Loading : NotificationIcon::Error
            )->show();
            return false;
        }
        // Leaving the Practice level for the Target: it may be kept loaded so
        // coming back is instant (see "keep-practice-loaded" in LevelSwap.cpp).
        swapToLevel(this, level, start, m_fields->isPractice);
        return true;
    }

    static std::string shorten(std::string const& name) {
        return name.size() > 20 ? name.substr(0, 19) + "." : name;
    }

    static void warnAboutOtherSwitcher() {
        static bool warned = false; // once per game session is enough
        if (warned) return;
        warned = true;
        // Notifications don't wrap text, so break the lines ourselves to fit the screen.
        Notification::create(
            "Another startpos switcher is on\n"
            "(like Mega Hack's). Turn one off\n"
            "or change keys in mod settings.",
            NotificationIcon::Warning,
            5.f
        )->show();
    }

    void switchTo(int index) {
        // Switching mid-transition can crash or confuse GD, so don't.
        if (m_isPaused || m_levelEndAnimationStarted || !m_player1) return;

        auto fields = m_fields.self();
        auto& list = fields->startPositions;
        if (index < 0 || index > static_cast<int>(list.size())) return;

        fields->index = index;
        StartPosObject* target = index == 0 ? nullptr : list[index - 1].data();

        // Test mode is what stops startpos runs from counting as real progress.
        // Starting from 0 is a normal attempt, so it's off there.
        m_isTestMode = target != nullptr;
        this->updateTestModeLabel();

        this->setActiveStartPosObject(target);
        // Timed and logged, so slow restarts show up in the Geode log.
        auto started = std::chrono::steady_clock::now();
        bool cached = this->hasSavedState(target);
        if (m_isPracticeMode) {
            this->resetLevelFromStart();
        }
        this->resetLevel();
        this->startMusic();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count();
        m_fields->lastRestartMs = static_cast<int>(ms);
        log::info("Switched to startpos {} of {} in {} ms{}", index, list.size(), ms, cached ? " (saved state)" : "");

        this->showIndexLabel();
    }

    // Redraws the "< 4/21 >" display at the bottom of the screen. It stays on
    // screen the whole time, but only on levels where there's somewhere to
    // switch to (startposes of its own, or a linked level).
    void showIndexLabel() {
        if (!m_uiLayer) return;
        auto fields = m_fields.self();
        int total = static_cast<int>(fields->startPositions.size());

        if (!fields->indicator) {
            fields->indicator = StartposIndicator::create([this](int direction) {
                // Clicking an arrow = pressing that switch key.
                if (isSwapInProgress() || m_fields->typing) return;
                this->switchBy(direction);
            });
            auto winSize = CCDirector::get()->getWinSize();
            fields->indicator->setPosition({ winSize.width / 2, 18.f });
            m_uiLayer->addChild(fields->indicator, 100);
        }
        fields->indicator->setVisible(total > 0 || fields->link.has_value());

        if (fields->isTarget) {
            // The Target has no startposes of its own; presses count toward
            // which Practice startpos to swap to (see switchBy).
            int n = fields->targetPresses;
            fields->indicator->setText(n == 0 ? std::string("Target")
                : n > 0 ? fmt::format("Practice #{}", n)
                : n == -1 ? std::string("Practice: last")
                : fmt::format("Practice: last - {}", -n - 1), "");
            return;
        }
        // Stop 0 is the start of the level, or the Target level on a Practice level.
        std::string left = fields->index != 0 ? std::to_string(fields->index)
            : fields->isPractice ? std::string("Target") : std::string("0");
        fields->indicator->setText(left, fmt::format("/{}", total));
    }

    // Freeze the level and start typing. Returns false if it can't start now.
    bool beginTyping() {
        auto fields = m_fields.self();
        if (fields->typing || fields->preparing || fields->isTarget) return false;
        if (m_isPaused || m_levelEndAnimationStarted || !m_player1 || isSwapInProgress()) return false;
        // Nothing to type on a level with no startposes (unless 0 = Target).
        if (fields->startPositions.empty() && !fields->isPractice) return false;

        // Drop any presses still waiting: the typed number replaces them.
        this->unschedule(schedule_selector(BSPlayLayer::applyPendingSwitch));
        fields->pending = false;
        fields->index = this->currentIndexInGame();

        fields->typing = true;
        fields->typed.clear();
        s_typingLayer = this; // freezes the level (BSFreeze) and grabs the keyboard
        this->pauseAudio();
        this->showIndexLabel();
        if (fields->indicator) fields->indicator->showTyping("");
        return true;
    }

    // A key was pressed while typing.
    void typeKey(enumKeyCodes key) {
        auto fields = m_fields.self();
        int digit = -1;
        if (key >= KEY_Zero && key <= KEY_Nine) digit = key - KEY_Zero;
        if (key >= KEY_NumPad0 && key <= KEY_NumPad9) digit = key - KEY_NumPad0;

        if (digit >= 0) {
            // A leading 0 gets replaced ("0" then "5" is 5, not 05).
            std::string next = (fields->typed == "0" ? "" : fields->typed) + std::to_string(digit);
            // Only numbers that exist can be typed: past the last startpos, the
            // digit is refused (the number flashes red).
            int max = static_cast<int>(fields->startPositions.size());
            if (next.size() > 4 || numFromString<int>(next).unwrapOr(max + 1) > max) {
                if (fields->indicator) fields->indicator->flashRejected();
                return;
            }
            fields->typed = next;
        }
        else if (key == KEY_Backspace || key == KEY_Delete) {
            if (!fields->typed.empty()) fields->typed.pop_back();
        }
        else if (key == KEY_Enter || key == KEY_NumEnter) {
            return this->submitTyping();
        }
        else if (key == KEY_Escape) {
            s_ignoreNextPause = true;
            Loader::get()->queueInMainThread([] { s_ignoreNextPause = false; });
            return this->cancelTyping();
        }
        if (fields->indicator) fields->indicator->showTyping(fields->typed);
    }

    // Enter or the go-to key: jump to the typed startpos (nothing typed = cancel).
    void submitTyping() {
        auto fields = m_fields.self();
        if (fields->typed.empty()) return this->cancelTyping();
        // Only digits can be typed, so this always parses; 0 is a safe fallback.
        int index = numFromString<int>(fields->typed).unwrapOr(0);
        this->endTyping();
        this->goTo(index);
    }

    void cancelTyping() {
        this->endTyping();
        this->showIndexLabel();
    }

    // Unfreeze the level and put the display back to normal.
    void endTyping() {
        auto fields = m_fields.self();
        if (!fields->typing) return;
        fields->typing = false;
        fields->typed.clear();
        if (s_typingLayer == this) s_typingLayer = nullptr;
        // Jump presses made while frozen would fire the moment the level resumes.
        m_queuedButtons.clear();
        this->resumeAudio();
        if (fields->indicator) fields->indicator->stopTyping();
    }

    void onQuit() {
        this->endTyping();
        PlayLayer::onQuit();
    }
};

// Freezes a level while you type a go-to number: GD moves the level forward
// in update(), so skipping it stops everything in place (like pausing, but
// with no pause menu on screen).
class $modify(BSFreeze, GJBaseGameLayer) {
    void update(float dt) {
        if (s_typingLayer && s_typingLayer == static_cast<GJBaseGameLayer*>(this)) return;
        GJBaseGameLayer::update(dt);
    }
};

// While typing, key presses go to the number instead of the game (so Space
// doesn't jump). Key releases still go through, so GD never thinks a key is
// stuck down. The go-to key itself is handled by its keybind before this.
class $modify(BSTypingKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (s_typingLayer && down) {
            if (!repeat) static_cast<BSPlayLayer*>(s_typingLayer)->typeKey(key);
            return true;
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

// Go-to keybind: freeze the level and type a startpos number into the switcher
// display, then go straight there with one restart (no cycling through the
// ones in between). Pressing it again while typing = go.
static bool onGoToKey(bool down, bool repeat) {
    if (!down || repeat) return false;
    if (s_typingLayer) {
        static_cast<BSPlayLayer*>(s_typingLayer)->submitTyping();
        return true;
    }
    auto playLayer = static_cast<BSPlayLayer*>(PlayLayer::get());
    if (!playLayer) return false;
    playLayer->refreshLinkRole();
    // Swallow the key only if typing started; otherwise let others use it.
    return playLayer->beginTyping();
}

static bool onSwitchKey(int delta, bool down, bool repeat) {
    if (!down || repeat) return false;
    if (isSwapInProgress() || s_typingLayer) return false;
    if (auto playLayer = PlayLayer::get()) {
        static_cast<BSPlayLayer*>(playLayer)->switchBy(delta);
    }
    // Don't swallow the key: other mods or GD may also use it.
    return false;
}

$on_mod(Loaded) {
    listenForKeybindSettingPresses("previous-startpos", [](Keybind const&, bool down, bool repeat, double) {
        return onSwitchKey(-1, down, repeat);
    });
    listenForKeybindSettingPresses("next-startpos", [](Keybind const&, bool down, bool repeat, double) {
        return onSwitchKey(1, down, repeat);
    });
    listenForKeybindSettingPresses("go-to-startpos", [](Keybind const&, bool down, bool repeat, double) {
        return onGoToKey(down, repeat);
    });
}
