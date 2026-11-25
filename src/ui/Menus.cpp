#include "Menus.hpp"
#include "LinksPopup.hpp"
#include "Rows.hpp"
#include "../StartDefaults.hpp"
#include "../LevelSwap.hpp"

using namespace geode::prelude;



HubPopup* HubPopup::create() {
    auto ret = new HubPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool HubPopup::init() {
    if (!Popup::init(260.f, 150.f)) return false;
    this->setTitle("Better Startpos");

    // One row of slots: icon button with its name underneath, centered.
    // To add a section, push it onto `slots` below; the row re-centers itself.
    // All buttons sit in the popup's own button menu. (They used to each have
    // a small menu nested inside a bigger menu, and clicks never reached them.)
    constexpr float SLOT_GAP = 100.f; // distance between slot centers
    constexpr float SLOT_Y = 6.f;     // relative to the popup's center

    struct Slot {
        char const* name;
        CCNode* icon;
        geode::Function<void()> onClick;
    };
    // Icons are GD's own buttons, sized to the same height.
    constexpr float ICON_HEIGHT = 52.f;
    auto icon = [](char const* frameName) {
        auto sprite = CCSprite::createWithSpriteFrameName(frameName);
        sprite->setScale(ICON_HEIGHT / sprite->getContentHeight());
        return sprite;
    };

    std::vector<Slot> slots;
    slots.push_back({ "Settings", icon("GJ_optionsBtn_001.png"),
        [] {
            auto playLayer = PlayLayer::get();
            SettingsPopup::create(playLayer ? playLayer->m_level : nullptr)->show();
        } });
    // GD's own link button only exists at low resolution and looked blurry at
    // this size, so it's our own chain icon (resources/link-icon.png) on
    // Geode's sharp green circle instead.
    slots.push_back({ "Links",
        CircleButtonSprite::create(CCSprite::create("link-icon.png"_spr), CircleBaseColor::Green, CircleBaseSize::Medium),
        [] { LinksPopup::create()->show(); } });

    float firstX = -SLOT_GAP * (static_cast<float>(slots.size()) - 1.f) / 2.f;
    for (size_t i = 0; i < slots.size(); i++) {
        CCPoint center = { firstX + SLOT_GAP * i, SLOT_Y };
        auto button = CCMenuItemExt::createSpriteExtra(slots[i].icon,
            [onClick = std::move(slots[i].onClick)](auto) mutable { onClick(); });
        m_buttonMenu->addChildAtPosition(button, Anchor::Center, center);

        auto label = CCLabelBMFont::create(slots[i].name, "bigFont.fnt");
        label->setScale(0.32f);
        m_mainLayer->addChildAtPosition(label, Anchor::Center, center + CCPoint { 0.f, -36.f });
    }
    return true;
}


static constexpr auto SMART_INFO =
    "Startposes are often placed with the wrong <cy>speed</c>, <cy>gamemode</c>, <cy>size</c>, "
    "<cy>gravity</c>, <cy>dual</c> or <cy>mirror</c> for their spot in the level. With this "
    "ticked, each startpos uses what the level's portals before it say instead, so you start "
    "the way you would when playing from 0.\n\n"
    "It only looks at the order of portals from left to right, so a portal off your path "
    "still counts.\n\n"
    "<cy>Gravity orbs and pads</c> depend on your clicks, so after one of those a startpos "
    "keeps its own gravity until you play past it once. From then on it uses the gravity "
    "you actually had there.\n\n"
    "The level itself is never changed.";

SettingsPopup* SettingsPopup::create(GJGameLevel* level) {
    auto ret = new SettingsPopup();
    if (ret->init(level)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

// Settings popups are 340 wide; rows use all but a 12-unit margin on each
// side, so the controls sit close to the left edge.
static constexpr float SETTINGS_POPUP_WIDTH = 340.f;
static constexpr float SETTINGS_WIDTH = SETTINGS_POPUP_WIDTH - 24.f;

bool SettingsPopup::init(GJGameLevel* level) {
    if (!Popup::init(SETTINGS_POPUP_WIDTH, 230.f)) return false;
    m_level = level;
    this->setTitle("Settings");

    // Tabs only make sense inside a level.
    if (m_level) {
        m_tabMenu = CCMenu::create();
        m_tabMenu->setContentSize({ 260.f, 30.f });
        m_tabMenu->setLayout(RowLayout::create()->setGap(8.f)->setAutoScale(false));
        m_mainLayer->addChildAtPosition(m_tabMenu, Anchor::Top, { 0.f, -50.f });
    }

    m_content = CCNode::create();
    m_content->setContentSize({ SETTINGS_WIDTH, 130.f });
    m_content->setAnchorPoint({ 0.5f, 0.5f });
    m_mainLayer->addChildAtPosition(m_content, Anchor::Center, { 0.f, m_level ? -22.f : -8.f });

    this->showTab(false);
    return true;
}

void SettingsPopup::showTab(bool thisLevel) {
    if (m_tabMenu) {
        m_tabMenu->removeAllChildren();
        auto addTab = [this](char const* name, bool tabIsThisLevel, bool active) {
            auto sprite = ButtonSprite::create(name, "bigFont.fnt", active ? "GJ_button_01.png" : "GJ_button_04.png", 0.5f);
            sprite->setScale(0.65f); // full size crowded the title and the rows
            m_tabMenu->addChild(CCMenuItemExt::createSpriteExtra(sprite, [this, tabIsThisLevel](auto) {
                // Rebuild next frame: we're inside this button's click.
                Loader::get()->queueInMainThread([self = Ref(this), tabIsThisLevel] { self->showTab(tabIsThisLevel); });
            }));
        };
        addTab("All levels", false, !thisLevel);
        addTab("This level", true, thisLevel);
        m_tabMenu->updateLayout();
    }

    m_content->removeAllChildren();
    float y = 110.f;
    auto addRow = [&](CCNode* row) {
        row->setPosition({ SETTINGS_WIDTH / 2, y });
        m_content->addChild(row);
        y -= 36.f;
    };

    if (!thisLevel) {
        // Each row saves only its own part, so one row can't undo another's change.
        addRow(DefaultStartRow::create(getGlobalDefault(), [](StartDefault value) {
            auto saved = getGlobalDefault();
            saved.mode = value.mode;
            saved.number = value.number;
            setGlobalDefault(saved);
        }, SETTINGS_WIDTH));

        addRow(makeCheckboxRow("Smart startpos", SMART_INFO, getGlobalDefault().smart, [](bool on) {
            auto saved = getGlobalDefault();
            saved.smart = on;
            setGlobalDefault(saved);
        }, SETTINGS_WIDTH));

        addRow(makeCheckboxRow("Fast SP on every Play",
            "Tick the <cy>Fast SP</c> box on every level's page by default. With it ticked, every "
            "startpos is prepared before the level starts, so switching is near-instant.",
            Mod::get()->getSettingValue<bool>("fast-sp-auto"),
            [](bool on) { Mod::get()->setSettingValue<bool>("fast-sp-auto", on); },
            SETTINGS_WIDTH));
    }
    else {
        auto levelDefault = getLevelDefault(m_level);
        auto shown = levelDefault.value_or(getGlobalDefault());

        // This level's rows only save while the level has its own settings.
        auto startRow = DefaultStartRow::create(shown, [level = m_level](StartDefault value) {
            if (auto saved = getLevelDefault(level)) {
                saved->mode = value.mode;
                saved->number = value.number;
                setLevelDefault(level, saved);
            }
        }, SETTINGS_WIDTH);
        auto smartRow = makeCheckboxRow("Smart startpos", SMART_INFO, shown.smart, [level = m_level](bool on) {
            if (auto saved = getLevelDefault(level)) {
                saved->smart = on;
                setLevelDefault(level, saved);
            }
        }, SETTINGS_WIDTH);

        addRow(makeCheckboxRow("Use all-levels setting",
            "When ticked, this level uses the <cy>All levels</c> settings below. "
            "Untick it to give this level its own.",
            !levelDefault.has_value(),
            [level = m_level, startRow = Ref(startRow), smartRow = Ref(smartRow)](bool useAll) {
                if (useAll) setLevelDefault(level, std::nullopt);
                else setLevelDefault(level, getGlobalDefault()); // start from a copy of All levels
                startRow->setEnabled(!useAll);
                smartRow->setEnabled(!useAll);
            }, SETTINGS_WIDTH));

        startRow->setEnabled(levelDefault.has_value());
        smartRow->setEnabled(levelDefault.has_value());
        addRow(startRow);
        addRow(smartRow);
    }

    // No "reopen the level" note: the level picks up changes when you
    // unpause (see applySettingsChanges in StartposSwitcher.cpp).
    handleTouchPriority(this);
}


LinksSettingsPopup* LinksSettingsPopup::create() {
    auto ret = new LinksSettingsPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LinksSettingsPopup::init() {
    if (!Popup::init(SETTINGS_POPUP_WIDTH, 170.f)) return false;
    this->setTitle("Links Settings");

    auto keepRow = makeCheckboxRow("Keep Practice loaded",
        "While you play the Target level, keep the Practice level loaded in the background, so "
        "swapping back is instant and prepared startposes stay prepared. Uses more memory. "
        "<cy>Needs Seamless swap.</c>",
        Mod::get()->getSettingValue<bool>("keep-practice-loaded"),
        [](bool on) {
            Mod::get()->setSettingValue<bool>("keep-practice-loaded", on);
            if (!on) dropParkedLevel(); // free it now, not at the next swap
        },
        SETTINGS_WIDTH);

    auto seamlessRow = makeCheckboxRow("Seamless swap",
        "When swapping between linked levels, keep the current level on screen while the other "
        "one loads, then cut straight to it (instead of a black loading screen).",
        Mod::get()->getSettingValue<bool>("seamless-swap"),
        [](bool on) {
            Mod::get()->setSettingValue<bool>("seamless-swap", on);
            if (!on) dropParkedLevel(); // keeping it loaded needs seamless swap
        },
        SETTINGS_WIDTH);

    m_mainLayer->addChildAtPosition(seamlessRow, Anchor::Center, { 0.f, 18.f });
    m_mainLayer->addChildAtPosition(keepRow, Anchor::Center, { 0.f, -18.f });
    handleTouchPriority(this);
    return true;
}
