#include "FastSP.hpp"
#include "LevelRef.hpp"
#include <Geode/modify/LevelInfoLayer.hpp>
#include <Geode/modify/EditLevelLayer.hpp>

using namespace geode::prelude;

// The checkbox's state per level, for this game session. A level without an
// entry follows the "Fast SP on every Play" setting.
static std::vector<std::pair<LevelRef, bool>> s_choices;

static std::optional<bool> choiceFor(GJGameLevel* level) {
    if (!level) return std::nullopt;
    auto ref = LevelRef::fromLevel(level);
    for (auto const& [choiceRef, on] : s_choices) {
        if (choiceRef.sameLevel(ref)) return on;
    }
    return std::nullopt;
}

static void setChoice(GJGameLevel* level, bool on) {
    auto ref = LevelRef::fromLevel(level);
    std::erase_if(s_choices, [&](auto const& entry) { return entry.first.sameLevel(ref); });
    s_choices.push_back({ ref, on });
}

static bool autoSetting() {
    return Mod::get()->getSettingValue<bool>("fast-sp-auto");
}

bool takeFastSPRequest(GJGameLevel* level) {
    return choiceFor(level).value_or(autoSetting());
}

static constexpr auto INFO_TEXT =
    "<cy>Fast SP</c> (fast startpos switching). "
    "Tick this, then press <cg>Play</c>: every startpos gets ready <cg>before</c> the level "
    "starts, so switching between them is <cg>near-instant</c> instead of taking a second or two.\n\n"
    "It takes a little while first: about as long as switching to every startpos once. "
    "You'll see a progress bar, and you can press <cy>Esc</c> to skip it.\n\n"
    "Turn on <cy>Fast SP on every Play</c> in the mod's Settings to have this "
    "ticked on every level by default.";

// Adds a row centered just under the page's Play button:
//   [checkbox] Fast SP (i)
// yOffset nudges it: the online page has little room above its progress bars.
// rowScale sets the row's height (the text and "i" fit it); checkboxScale sizes
// the checkbox itself, a touch smaller than the row on the busier online page.
static void addFastSPCheckbox(CCNode* page, GJGameLevel* level, float yOffset, float rowScale, float checkboxScale) {
    auto playButton = page->getChildByIDRecursive("play-button");
    if (!playButton || !playButton->getParent()) return; // page layout we don't recognize: skip

    auto menu = CCMenu::create();
    menu->setID("fast-sp-menu"_spr);
    menu->setContentSize({ 160.f, 24.f });
    menu->ignoreAnchorPointForPosition(false);
    menu->setAnchorPoint({ 0.5f, 1.f }); // positioned by its top-center
    // Auto-scale off: it assumes every child's intended scale is 1, which blew
    // the small label back up to full size.
    menu->setLayout(RowLayout::create()->setGap(4.f)->setAutoScale(false));

    // The row's height comes from a checkbox at rowScale; the text and the "i"
    // are sized to fit it, so all three line up.
    float rowHeight = CCSprite::createWithSpriteFrameName("GJ_checkOff_001.png")->getContentSize().height * rowScale;

    auto checkbox = CCMenuItemExt::createTogglerWithStandardSprites(checkboxScale, [level = Ref(level)](CCMenuItemToggler* toggler) {
        // GD calls this before flipping the checkbox, so the new state is the opposite.
        setChoice(level, !toggler->isToggled());
    });
    checkbox->setID("fast-sp-toggle"_spr);
    checkbox->toggle(takeFastSPRequest(level));
    menu->addChild(checkbox);

    auto label = CCLabelBMFont::create("Fast SP", "bigFont.fnt");
    label->setScale(rowHeight / label->getContentSize().height);
    menu->addChild(label);

    float infoHeight = CCSprite::createWithSpriteFrameName("GJ_infoIcon_001.png")->getContentSize().height;
    menu->addChild(InfoAlertButton::create("Fast SP", INFO_TEXT, rowHeight / infoHeight));
    menu->updateLayout();

    auto playWorld = playButton->getParent()->convertToWorldSpace(playButton->getPosition());
    auto playPos = page->convertToNodeSpace(playWorld);
    auto playSize = playButton->getScaledContentSize();
    menu->setPosition({ playPos.x, playPos.y - playSize.height / 2 + yOffset });
    page->addChild(menu, 10);
    handleTouchPriority(page);
}

// Online levels' page.
class $modify(BSLevelInfoLayer, LevelInfoLayer) {
    bool init(GJGameLevel* level, bool challenge) {
        if (!LevelInfoLayer::init(level, challenge)) return false;
        addFastSPCheckbox(this, level, 1.f, 0.45f, 0.4f);
        return true;
    }
};

// Your own (local) levels' page.
class $modify(BSEditLevelLayer, EditLevelLayer) {
    bool init(GJGameLevel* level) {
        if (!EditLevelLayer::init(level)) return false;
        addFastSPCheckbox(this, level, -2.f, 0.55f, 0.55f);
        return true;
    }
};
