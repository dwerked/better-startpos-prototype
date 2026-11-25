#pragma once
// Building blocks for the mod's menus, so every option looks the same.
// Every option row is laid out in three parts:
//
//     [ control ]  Name (i)
//
// The control column has the same width in every row, so all the names start
// at the same spot (one column), and a small control like a checkbox sits in
// the middle of it. Every row ends with an "i" button explaining the option.
//   Toggles:  [checkbox]   Name (i)
//   Choices:  < Value >    Name (i)   (arrows cycle the value)

#include <Geode/Geode.hpp>
#include <Geode/ui/TextInput.hpp>
#include "../StartDefaults.hpp"

// Width of the control column, and the height of one row.
inline constexpr float ROW_CONTROL_WIDTH = 80.f;
inline constexpr float ROW_HEIGHT = 30.f;

// A toggle row. `onChange` gets the new value. Positioned by its center.
class CheckboxRow : public cocos2d::CCNode {
public:
    static CheckboxRow* create(std::string const& name, std::string const& info, bool value,
                               geode::Function<void(bool)> onChange, float width = 260.f);
    void setEnabled(bool enabled); // greyed out and unclickable when false
    void setValue(bool value);     // ticks/unticks without calling onChange

protected:
    bool init(std::string const& name, std::string const& info, bool value,
              geode::Function<void(bool)> onChange, float width);

    CCMenuItemToggler* m_checkbox = nullptr;
    cocos2d::CCLabelBMFont* m_name = nullptr;
};

CheckboxRow* makeCheckboxRow(
    std::string const& name, std::string const& info, bool value,
    geode::Function<void(bool)> onChange, float width = 260.f
);

// "< Last >   Default startpos (i)". When the value is Number, a number box
// takes the place of the word between the arrows.
class DefaultStartRow : public cocos2d::CCNode {
public:
    static DefaultStartRow* create(StartDefault value, geode::Function<void(StartDefault)> onChange, float width = 260.f);
    void setEnabled(bool enabled); // greyed out and unclickable when false

protected:
    bool init(StartDefault value, geode::Function<void(StartDefault)> onChange, float width);
    void cycle(int direction);
    void refresh();

    StartDefault m_value;
    geode::Function<void(StartDefault)> m_onChange;
    cocos2d::CCLabelBMFont* m_valueLabel = nullptr;
    cocos2d::CCLabelBMFont* m_name = nullptr;
    geode::TextInput* m_numberInput = nullptr;
    CCMenuItemSpriteExtra* m_prev = nullptr;
    CCMenuItemSpriteExtra* m_next = nullptr;
    bool m_enabled = true;
};
