#pragma once
// The switcher display at the bottom of the screen while you play:
//
//     <   4/21   >
//
// The two arrows are buttons (clicking one works like the Left/Right keys).
// The text is in two parts, so that while you type a number for the go-to
// key, the left part can be replaced by what you typed plus a blinking cursor.

#include <Geode/Geode.hpp>

class StartposIndicator : public cocos2d::CCNode {
public:
    // onArrow gets -1 for the left arrow and +1 for the right one.
    static StartposIndicator* create(geode::Function<void(int)> onArrow);

    // left: "4", "Target"...; right: "/21", or "" for text with no total.
    void setText(std::string const& left, std::string const& right);

    // Typing mode (go-to key): the left part shows `typed` followed by a
    // blinking cursor. Call again each time `typed` changes.
    void showTyping(std::string const& typed);
    void stopTyping();

    // Briefly turns the typed text red: that digit isn't allowed.
    void flashRejected();

protected:
    bool init(geode::Function<void(int)> onArrow);
    void relayout();

    geode::Function<void(int)> m_onArrow;
    cocos2d::CCMenu* m_menu = nullptr;
    CCMenuItemSpriteExtra* m_prev = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
    cocos2d::CCLabelBMFont* m_left = nullptr;
    cocos2d::CCLabelBMFont* m_right = nullptr;
    cocos2d::CCLayerColor* m_cursor = nullptr;
    std::string m_leftText;   // what the left part shows when not typing
    bool m_typing = false;
};
