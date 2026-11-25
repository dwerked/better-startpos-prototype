#pragma once
// The Links screen (opened from the pause menu) and the level picker it opens.

#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <Geode/ui/TextInput.hpp>
#include "../LevelRef.hpp"

// Lists your links, lets you delete them, and has a + button for a new one.
class LinksPopup : public geode::Popup {
public:
    static LinksPopup* create();
    void refreshList();

protected:
    bool init();

    geode::ScrollLayer* m_list = nullptr;
};

// Pick two levels to link. The sort button (left of the search bar) chooses
// which of GD's level lists to show, in GD's own order:
//   Saved     - downloaded levels, most recently played first
//   Favorites - saved levels you hearted
//   Created   - your local levels
// The search box filters that list by name or ID. Picks stay picked when you
// change lists, so you can take one level from each.
class LinkPickerPopup : public geode::Popup {
public:
    static LinkPickerPopup* create(LinksPopup* parent);

protected:
    enum class Tab { Saved, Favorites, Created };

    struct Candidate {
        LevelRef ref;
        geode::Ref<GJGameLevel> level;
    };

    bool init(LinksPopup* parent);
    void setTab(Tab tab);
    void onSort();
    std::vector<Candidate> levelsInTab() const;
    // keepScroll: stay where you were (after picking) instead of jumping to the top.
    void refreshList(bool keepScroll = false);
    bool isPicked(LevelRef const& ref) const;
    void togglePick(Candidate const& candidate);
    void updateFooter();
    void onLink();
    void confirmLink(Candidate const& practice, Candidate const& target);
    void finishLink(Candidate const& practice, Candidate const& target);

    LinksPopup* m_parent = nullptr;
    Tab m_tab = Tab::Saved;
    std::vector<Candidate> m_tabLevels; // levels in the current tab
    std::vector<Candidate> m_picked;    // max 2, can come from different tabs
    std::string m_search;

    geode::ScrollLayer* m_list = nullptr;
    geode::TextInput* m_searchInput = nullptr;
    cocos2d::CCLabelBMFont* m_footerLabel = nullptr;
    CCMenuItemSpriteExtra* m_linkButton = nullptr;
};
