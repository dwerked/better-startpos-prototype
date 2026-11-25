#include "Rows.hpp"
#include <charconv>

using namespace geode::prelude;

static constexpr float NAME_SCALE = 0.38f;  // every option name uses this size
static constexpr float NAME_GAP = 8.f;      // space between the control column and the name
static constexpr float INFO_GAP = 12.f;     // space between the name and the "i" center
static constexpr float INFO_SCALE = 0.6f;
static constexpr float INFO_ROOM = 24.f;    // width kept free for the "i" after the name

// The shared skeleton of every row: an empty node of the row's size with a
// menu over it (for the buttons) and the name in the name column. Returns the
// name label; the "i" is placed right after it.
static CCLabelBMFont* buildRowSkeleton(CCNode* row, CCMenu*& menu, std::string const& name,
                                       std::string const& info, float width) {
    row->setContentSize({ width, ROW_HEIGHT });
    row->setAnchorPoint({ 0.5f, 0.5f });

    menu = CCMenu::create();
    menu->setContentSize(row->getContentSize());
    menu->setPosition({ 0.f, 0.f }); // CCMenu is positioned by its bottom-left corner
    row->addChild(menu);

    float nameX = ROW_CONTROL_WIDTH + NAME_GAP;
    auto label = CCLabelBMFont::create(name.c_str(), "bigFont.fnt");
    label->setAnchorPoint({ 0.f, 0.5f });
    label->setPosition({ nameX, ROW_HEIGHT / 2 });
    // Long names shrink to fit instead of running off the popup.
    limitNodeWidth(label, width - nameX - INFO_ROOM, NAME_SCALE, 0.1f);
    row->addChild(label);

    auto infoButton = InfoAlertButton::create(name, info, INFO_SCALE);
    infoButton->setPosition({ nameX + label->getScaledContentSize().width + INFO_GAP, ROW_HEIGHT / 2 });
    menu->addChild(infoButton);
    return label;
}

CheckboxRow* CheckboxRow::create(std::string const& name, std::string const& info, bool value,
                                 geode::Function<void(bool)> onChange, float width) {
    auto ret = new CheckboxRow();
    if (ret->init(name, info, value, std::move(onChange), width)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

CheckboxRow* makeCheckboxRow(std::string const& name, std::string const& info, bool value,
                             geode::Function<void(bool)> onChange, float width) {
    return CheckboxRow::create(name, info, value, std::move(onChange), width);
}

bool CheckboxRow::init(std::string const& name, std::string const& info, bool value,
                       geode::Function<void(bool)> onChange, float width) {
    if (!CCNode::init()) return false;
    CCMenu* menu = nullptr;
    m_name = buildRowSkeleton(this, menu, name, info, width);

    m_checkbox = CCMenuItemExt::createTogglerWithStandardSprites(0.6f,
        [onChange = std::move(onChange)](CCMenuItemToggler* toggler) mutable {
            // GD calls this before flipping the checkbox, so the new state is the opposite.
            onChange(!toggler->isToggled());
        });
    m_checkbox->toggle(value);
    // In the middle of the control column, so the name lines up with other rows.
    m_checkbox->setPosition({ ROW_CONTROL_WIDTH / 2, ROW_HEIGHT / 2 });
    menu->addChild(m_checkbox);
    return true;
}

void CheckboxRow::setValue(bool value) {
    m_checkbox->toggle(value);
}

void CheckboxRow::setEnabled(bool enabled) {
    m_checkbox->setEnabled(enabled);
    // Greyed out, but the "i" stays usable so you can read what it does.
    GLubyte opacity = enabled ? 255 : 100;
    m_name->setOpacity(opacity);
    for (auto sprite : { m_checkbox->m_offButton, m_checkbox->m_onButton }) {
        if (sprite) sprite->setOpacity(opacity);
    }
}

static constexpr auto DEFAULT_START_INFO =
    "Where a level starts when you open it.\n\n"
    "<cy>Last</c>: the last startpos (what GD does on its own).\n"
    "<cy>First</c>: the first startpos.\n"
    "<cy>From 0</c>: the start of the level, as a normal attempt.\n"
    "<cy>Number</c>: the startpos number you type in the box. "
    "If a level has fewer, it starts at its last one.";

DefaultStartRow* DefaultStartRow::create(StartDefault value, geode::Function<void(StartDefault)> onChange, float width) {
    auto ret = new DefaultStartRow();
    if (ret->init(value, std::move(onChange), width)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool DefaultStartRow::init(StartDefault value, geode::Function<void(StartDefault)> onChange, float width) {
    if (!CCNode::init()) return false;
    m_value = value;
    m_onChange = std::move(onChange);

    CCMenu* menu = nullptr;
    m_name = buildRowSkeleton(this, menu, "Default startpos", DEFAULT_START_INFO, width);

    // Control column: < value >, the arrows at its two ends.
    float middle = ROW_CONTROL_WIDTH / 2;
    auto makeArrow = [this](bool next) {
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        sprite->setScale(0.4f);
        if (next) sprite->setFlipX(true);
        return CCMenuItemExt::createSpriteExtra(sprite, [this, next](auto) { this->cycle(next ? 1 : -1); });
    };
    m_prev = makeArrow(false);
    m_next = makeArrow(true);
    m_prev->setPosition({ 8.f, ROW_HEIGHT / 2 });
    m_next->setPosition({ ROW_CONTROL_WIDTH - 8.f, ROW_HEIGHT / 2 });
    menu->addChild(m_prev);
    menu->addChild(m_next);

    m_valueLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_valueLabel->setPosition({ middle, ROW_HEIGHT / 2 });
    this->addChild(m_valueLabel);

    // Number mode: this box takes the place of the word between the arrows.
    m_numberInput = TextInput::create(50.f, "#");
    m_numberInput->setCommonFilter(CommonFilter::Uint);
    m_numberInput->setMaxCharCount(4);
    m_numberInput->setScale(0.75f);
    m_numberInput->setString(std::to_string(m_value.number));
    m_numberInput->setCallback([this](std::string const& text) {
        int number = 0;
        auto [_, error] = std::from_chars(text.data(), text.data() + text.size(), number);
        if (error != std::errc() || number < 1) return; // empty or 0: keep the old number
        m_value.number = number;
        m_onChange(m_value);
    });
    m_numberInput->setPosition({ middle, ROW_HEIGHT / 2 });
    this->addChild(m_numberInput);

    this->refresh();
    return true;
}

void DefaultStartRow::cycle(int direction) {
    if (!m_enabled) return;
    int mode = (static_cast<int>(m_value.mode) + direction + 4) % 4;
    m_value.mode = static_cast<StartDefault::Mode>(mode);
    this->refresh();
    m_onChange(m_value);
}

void DefaultStartRow::refresh() {
    bool numberMode = m_value.mode == StartDefault::Mode::Number;
    m_valueLabel->setString(m_value.describe().c_str());
    // Room between the two arrows.
    limitNodeWidth(m_valueLabel, ROW_CONTROL_WIDTH - 30.f, NAME_SCALE, 0.1f);
    m_valueLabel->setVisible(!numberMode);
    m_numberInput->setVisible(numberMode);
}

void DefaultStartRow::setEnabled(bool enabled) {
    m_enabled = enabled;
    m_prev->setEnabled(enabled);
    m_next->setEnabled(enabled);
    m_numberInput->setEnabled(enabled);
    // Greyed out, but the "i" stays usable so you can read what it does.
    GLubyte opacity = enabled ? 255 : 100;
    m_valueLabel->setOpacity(opacity);
    m_name->setOpacity(opacity);
    for (auto arrow : { m_prev, m_next }) {
        if (auto sprite = typeinfo_cast<CCSprite*>(arrow->getNormalImage())) sprite->setOpacity(opacity);
    }
}
