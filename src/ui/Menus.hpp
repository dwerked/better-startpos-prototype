#pragma once
// The mod's menus, opened from its pause-menu button:
//   HubPopup           a row of sections: Settings, Links
//   SettingsPopup      general settings; tabs "All levels" / "This level"
//   LinksSettingsPopup the Links section's own settings (gear on the Links screen)

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

class HubPopup : public geode::Popup {
public:
    static HubPopup* create();
protected:
    bool init();
};

class SettingsPopup : public geode::Popup {
public:
    // `level`: the level you're in (enables the "This level" tab), or nullptr.
    static SettingsPopup* create(GJGameLevel* level);
protected:
    bool init(GJGameLevel* level);
    void showTab(bool thisLevel);

    geode::Ref<GJGameLevel> m_level;
    cocos2d::CCMenu* m_tabMenu = nullptr;
    cocos2d::CCNode* m_content = nullptr;
};

class LinksSettingsPopup : public geode::Popup {
public:
    static LinksSettingsPopup* create();
protected:
    bool init();
};
