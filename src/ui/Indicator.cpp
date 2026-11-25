#include "Indicator.hpp"

using namespace geode::prelude;

// Sizes, in GD's screen units (the screen is about 570 x 320 of these).
static constexpr float TEXT_SCALE = 0.6f;
static constexpr float ARROW_SCALE = 0.45f;
static constexpr float ARROW_GAP = 14.f;     // space between the text and each arrow
static constexpr float CURSOR_WIDTH = 2.5f;
static constexpr float CURSOR_GAP = 2.f;     // space on each side of the cursor

StartposIndicator* StartposIndicator::create(geode::Function<void(int)> onArrow) {
    auto ret = new StartposIndicator();
    if (ret->init(std::move(onArrow))) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool StartposIndicator::init(geode::Function<void(int)> onArrow) {
    if (!CCNode::init()) return false;
    m_onArrow = std::move(onArrow);
    this->setID("startpos-indicator"_spr);

    // Everything is placed around this node's position (0, 0) = the middle of the text.
    m_menu = CCMenu::create();
    m_menu->setPosition({ 0.f, 0.f });
    this->addChild(m_menu);

    // GD's own green arrow (the one on its "back" buttons), flipped for "next".
    auto makeArrow = [this](int direction) {
        auto sprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
        sprite->setScale(ARROW_SCALE);
        if (direction > 0) sprite->setFlipX(true);
        return CCMenuItemExt::createSpriteExtra(sprite, [this, direction](auto) {
            if (m_onArrow) m_onArrow(direction);
        });
    };
    m_prev = makeArrow(-1);
    m_next = makeArrow(1);
    m_menu->addChild(m_prev);
    m_menu->addChild(m_next);

    m_left = CCLabelBMFont::create("", "bigFont.fnt");
    m_left->setScale(TEXT_SCALE);
    m_left->setAnchorPoint({ 0.f, 0.5f }); // placed by its left edge in relayout
    this->addChild(m_left);

    m_right = CCLabelBMFont::create("", "bigFont.fnt");
    m_right->setScale(TEXT_SCALE);
    m_right->setAnchorPoint({ 0.f, 0.5f });
    this->addChild(m_right);

    // The text cursor: a thin white bar, as tall as the digits.
    m_cursor = CCLayerColor::create({ 255, 255, 255, 255 }, CURSOR_WIDTH, 32.f * TEXT_SCALE * 0.75f);
    m_cursor->ignoreAnchorPointForPosition(false);
    m_cursor->setAnchorPoint({ 0.f, 0.5f });
    m_cursor->setVisible(false);
    this->addChild(m_cursor);

    return true;
}

void StartposIndicator::setText(std::string const& left, std::string const& right) {
    m_leftText = left;
    if (!m_typing) m_left->setString(left.c_str());
    m_right->setString(right.c_str());
    this->relayout();
}

void StartposIndicator::showTyping(std::string const& typed) {
    if (!m_typing) {
        m_typing = true;
        // Blink: half a second on, half a second off, until typing ends.
        m_cursor->setVisible(true);
        m_cursor->stopAllActions();
        m_cursor->runAction(CCRepeatForever::create(CCSequence::create(
            CCDelayTime::create(0.5f), CCHide::create(),
            CCDelayTime::create(0.5f), CCShow::create(),
            nullptr)));
    }
    m_left->setString(typed.c_str());
    this->relayout();
}

void StartposIndicator::stopTyping() {
    m_typing = false;
    m_cursor->stopAllActions();
    m_cursor->setVisible(false);
    m_left->stopAllActions();
    m_left->setColor({ 255, 255, 255 });
    m_left->setString(m_leftText.c_str());
    this->relayout();
}

void StartposIndicator::flashRejected() {
    m_left->stopAllActions();
    m_left->setColor({ 255, 90, 90 });
    m_left->runAction(CCTintTo::create(0.35f, 255, 255, 255));
}

// Lines up [<] [left][cursor][right] [>] centered on this node.
void StartposIndicator::relayout() {
    // An empty label still reports a little width; count it as none.
    float leftWidth = m_left->getString()[0] ? m_left->getScaledContentSize().width : 0.f;
    float rightWidth = m_right->getString()[0] ? m_right->getScaledContentSize().width : 0.f;
    float cursorWidth = m_typing ? CURSOR_WIDTH + CURSOR_GAP * 2 : 0.f;
    float total = leftWidth + cursorWidth + rightWidth;

    float x = -total / 2;
    m_left->setPosition({ x, 0.f });
    x += leftWidth;
    m_cursor->setPosition({ x + CURSOR_GAP, 0.f });
    x += cursorWidth;
    m_right->setPosition({ x, 0.f });

    float arrowOffset = total / 2 + ARROW_GAP + m_prev->getScaledContentSize().width / 2;
    m_prev->setPosition({ -arrowOffset, 0.f });
    m_next->setPosition({ arrowOffset, 0.f });
}
