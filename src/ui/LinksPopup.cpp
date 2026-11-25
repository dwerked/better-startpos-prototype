#include "LinksPopup.hpp"
#include "../Links.hpp"
#include "Menus.hpp"

using namespace geode::prelude;


// A dark rounded box, used behind lists and rows.
static NineSlice* makeBox(CCSize size, GLubyte opacity) {
    auto box = NineSlice::create("square02b_001.png");
    box->setContentSize(size);
    box->setColor({ 0, 0, 0 });
    box->setOpacity(opacity);
    return box;
}

// Space between the list's dark background and its rows, on every side.
// Rows are LIST_PAD * 2 narrower than the list; the scrolling area is
// LIST_PAD * 2 shorter than the background, so the top and bottom match.
static constexpr float LIST_PAD = 5.f;

// A scrolling list with a dark background, centered in the popup.
static ScrollLayer* addScrollList(CCNode* parent, CCSize size, float yOffset) {
    auto bg = makeBox(size, 90);
    parent->addChildAtPosition(bg, Anchor::Center, { 0, yOffset });

    CCSize scrollSize = { size.width, size.height - LIST_PAD * 2 };
    auto list = ScrollLayer::create(scrollSize);
    list->m_contentLayer->setLayout(ScrollLayer::createDefaultListLayout(3.f));
    // Position it by its centre, set explicitly: Geode 5 placed a ScrollLayer
    // by its bottom-left corner and Geode 6 by its centre, and assuming either
    // one put the list half its size off.
    list->ignoreAnchorPointForPosition(false);
    list->setAnchorPoint({ 0.5f, 0.5f });
    parent->addChildAtPosition(list, Anchor::Center, { 0.f, yOffset });
    return list;
}

static CCLabelBMFont* makeLabel(std::string const& text, char const* font, float scale, float maxWidth) {
    auto label = CCLabelBMFont::create(text.c_str(), font);
    label->setAnchorPoint({ 0, 0.5f });
    limitNodeWidth(label, maxWidth, scale, 0.1f);
    return label;
}

// The name to show for a saved ref: the level's current name if we can find it.
static std::string displayName(LevelRef const& ref, bool& found) {
    auto level = ref.resolve();
    found = level != nullptr;
    return level ? std::string(level->m_levelName) : ref.name;
}


static constexpr CCSize LINKS_LIST_SIZE = { 360.f, 175.f };

LinksPopup* LinksPopup::create() {
    auto ret = new LinksPopup();
    if (ret->init()) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LinksPopup::init() {
    if (!Popup::init(400.f, 260.f)) return false;
    this->setTitle("Linked Levels");

    m_list = addScrollList(m_mainLayer, LINKS_LIST_SIZE, -5.f);

    auto addButton = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_plusBtn_001.png", 0.6f, [this](auto) {
        LinkPickerPopup::create(this)->show();
    });
    addButton->setID("new-link-button"_spr);
    m_buttonMenu->addChildAtPosition(addButton, Anchor::BottomRight, { -22.f, 22.f });

    // This section's own settings.
    auto gear = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_optionsBtn_001.png", 0.55f, [](auto) {
        LinksSettingsPopup::create()->show();
    });
    gear->setID("links-settings-button"_spr);
    m_buttonMenu->addChildAtPosition(gear, Anchor::TopRight, { -24.f, -24.f });

    this->refreshList();
    return true;
}

void LinksPopup::refreshList() {
    auto content = m_list->m_contentLayer;
    content->removeAllChildren();

    auto links = loadLinks();
    if (links.empty()) {
        auto empty = CCLabelBMFont::create(
            "No links yet.\nPress + to link a practice copy\nto the level you're trying to beat.",
            "bigFont.fnt"
        );
        empty->setScale(0.4f);
        empty->setAlignment(kCCTextAlignmentCenter);
        // Put the message in a full-height node so the layout centers it.
        auto holder = CCNode::create();
        holder->setContentSize({ LINKS_LIST_SIZE.width, LINKS_LIST_SIZE.height - LIST_PAD * 2 });
        holder->addChildAtPosition(empty, Anchor::Center);
        content->addChild(holder);
    }

    constexpr float rowHeight = 40.f;
    for (size_t i = 0; i < links.size(); i++) {
        auto const& link = links[i];
        auto row = CCNode::create();
        row->setContentSize({ LINKS_LIST_SIZE.width - LIST_PAD * 2, rowHeight });
        row->addChildAtPosition(makeBox(row->getContentSize(), 70), Anchor::Center);

        bool practiceFound, targetFound;
        auto practiceName = displayName(link.practice, practiceFound);
        auto targetName = displayName(link.target, targetFound);

        auto practiceLabel = makeLabel(
            fmt::format("Practice: {}{}", practiceName, practiceFound ? "" : " (not found)"),
            "bigFont.fnt", 0.4f, 290.f
        );
        if (!practiceFound) practiceLabel->setColor({ 255, 120, 120 });
        row->addChildAtPosition(practiceLabel, Anchor::Left, { 8.f, 8.f });

        auto targetLabel = makeLabel(
            fmt::format("Target: {}{}", targetName, targetFound ? "" : " (not found)"),
            "bigFont.fnt", 0.4f, 290.f
        );
        targetLabel->setColor(targetFound ? ccColor3B{ 120, 255, 120 } : ccColor3B{ 255, 120, 120 });
        row->addChildAtPosition(targetLabel, Anchor::Left, { 8.f, -8.f });

        auto menu = CCMenu::create();
        menu->setContentSize(row->getContentSize());
        menu->ignoreAnchorPointForPosition(false);
        auto deleteButton = CCMenuItemExt::createSpriteExtraWithFrameName("GJ_trashBtn_001.png", 0.55f, [this, i](auto) {
            createQuickPopup(
                "Delete link?",
                "This only removes the link. Both levels stay as they are.",
                "Cancel", "Delete",
                [this, i](auto, bool confirmed) {
                    if (!confirmed) return;
                    removeLink(i);
                    this->refreshList();
                }
            );
        });
        menu->addChildAtPosition(deleteButton, Anchor::Right, { -20.f, 0.f });
        row->addChildAtPosition(menu, Anchor::Center);

        content->addChild(row);
    }

    content->updateLayout();
    m_list->scrollToTop();
    handleTouchPriority(this);
}


static constexpr CCSize PICKER_LIST_SIZE = { 380.f, 165.f };
static constexpr size_t MAX_ROWS_SHOWN = 150;

LinkPickerPopup* LinkPickerPopup::create(LinksPopup* parent) {
    auto ret = new LinkPickerPopup();
    if (ret->init(parent)) {
        ret->autorelease();
        return ret;
    }
    delete ret;
    return nullptr;
}

bool LinkPickerPopup::init(LinksPopup* parent) {
    if (!Popup::init(420.f, 290.f)) return false;
    m_parent = parent;
    this->setTitle("Pick 2 levels to link");

    // Sort button on the left, search bar to its right.
    auto sortSprite = CircleButtonSprite::createWithSpriteFrameName(
        "GJ_sortIcon_001.png", 1.f, CircleBaseColor::Green, CircleBaseSize::Small
    );
    auto sortButton = CCMenuItemExt::createSpriteExtra(sortSprite, [this](auto) { this->onSort(); });
    sortButton->setID("sort-button"_spr);
    m_buttonMenu->addChildAtPosition(sortButton, Anchor::TopLeft, { 38.f, -60.f });

    m_searchInput = TextInput::create(320.f, "Search");
    m_searchInput->setCommonFilter(CommonFilter::Any);
    m_searchInput->setCallback([this](std::string const& text) {
        m_search = text;
        this->refreshList();
    });
    m_mainLayer->addChildAtPosition(m_searchInput, Anchor::Top, { 25.f, -60.f });

    m_list = addScrollList(m_mainLayer, PICKER_LIST_SIZE, -22.f);

    m_footerLabel = CCLabelBMFont::create("", "bigFont.fnt");
    m_mainLayer->addChildAtPosition(m_footerLabel, Anchor::Bottom, { -40.f, 22.f });

    auto linkSprite = ButtonSprite::create("Link", "goldFont.fnt", "GJ_button_01.png", 0.8f);
    m_linkButton = CCMenuItemExt::createSpriteExtra(linkSprite, [this](auto) { this->onLink(); });
    m_buttonMenu->addChildAtPosition(m_linkButton, Anchor::BottomRight, { -50.f, 22.f });

    this->setTab(Tab::Saved);
    this->updateFooter();
    return true;
}

static char const* tabName(int tab) {
    switch (tab) {
        case 1: return "Favorites";
        case 2: return "Created";
        default: return "Saved";
    }
}

void LinkPickerPopup::setTab(Tab tab) {
    m_tab = tab;
    m_tabLevels = this->levelsInTab();
    // The title and placeholder show which list you're looking at.
    m_searchInput->setPlaceholder(fmt::format("Search {} levels", tabName(static_cast<int>(tab))));
    this->setTitle(fmt::format("Pick 2 levels: {}", tabName(static_cast<int>(tab))));
    this->refreshList();
}

// A small popup with one button per list. The current list is green.
class SortPopup : public Popup {
public:
    static SortPopup* create(int current, geode::Function<void(int)> onPick) {
        auto ret = new SortPopup();
        if (ret->init(current, std::move(onPick))) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }

protected:
    geode::Function<void(int)> m_onPick;

    bool init(int current, geode::Function<void(int)> onPick) {
        if (!Popup::init(200.f, 170.f)) return false;
        m_onPick = std::move(onPick);
        this->setTitle("Show levels from");

        auto menu = CCMenu::create();
        menu->setContentSize({ 160.f, 110.f });
        // Auto-scale off so the buttons keep the size we gave them.
        menu->setLayout(ColumnLayout::create()->setGap(6.f)->setAxisReverse(true)->setAutoScale(false));
        for (int tab = 0; tab < 3; tab++) {
            auto sprite = ButtonSprite::create(tabName(tab), 120, 120, 0.7f, true, "bigFont.fnt",
                tab == current ? "GJ_button_01.png" : "GJ_button_04.png", 30.f);
            menu->addChild(CCMenuItemExt::createSpriteExtra(sprite, [this, tab](auto) {
                m_onPick(tab);
                this->onClose(nullptr);
            }));
        }
        menu->updateLayout();
        m_mainLayer->addChildAtPosition(menu, Anchor::Center, { 0.f, -12.f });
        handleTouchPriority(this);
        return true;
    }
};

void LinkPickerPopup::onSort() {
    SortPopup::create(static_cast<int>(m_tab), [this](int tab) {
        this->setTab(static_cast<Tab>(tab));
    })->show();
}

std::vector<LinkPickerPopup::Candidate> LinkPickerPopup::levelsInTab() const {
    // Each tab uses the same list GD shows in its own menus, in the same order.
    CCArray* levels = nullptr;
    switch (m_tab) {
        case Tab::Saved: levels = GameLevelManager::get()->getSavedLevels(false, 0); break;
        case Tab::Favorites: levels = GameLevelManager::get()->getSavedLevels(true, 0); break;
        case Tab::Created: levels = LocalLevelManager::get()->m_localLevels; break;
    }

    std::vector<Candidate> result;
    if (!levels) return result;
    for (auto level : CCArrayExt<GJGameLevel*>(levels)) {
        // Built-in RobTop levels can't be linked.
        if (level->m_levelType == GJLevelType::Main || level->m_levelType == GJLevelType::Default) continue;
        result.push_back({ LevelRef::fromLevel(level), level });
    }
    return result;
}

static std::string toLower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return std::tolower(c); });
    return text;
}

bool LinkPickerPopup::isPicked(LevelRef const& ref) const {
    return std::any_of(m_picked.begin(), m_picked.end(), [&](Candidate const& c) { return c.ref.sameLevel(ref); });
}

void LinkPickerPopup::refreshList(bool keepScroll) {
    auto content = m_list->m_contentLayer;
    float oldY = content->getPositionY();
    content->removeAllChildren();

    auto query = toLower(m_search);
    constexpr float rowHeight = 30.f;
    size_t shown = 0;

    for (auto const& candidate : m_tabLevels) {
        if (shown >= MAX_ROWS_SHOWN) break;
        std::string name = candidate.level->m_levelName;

        if (!query.empty()) {
            bool nameMatches = toLower(name).find(query) != std::string::npos;
            bool idMatches = !candidate.ref.isLocal && std::to_string(candidate.ref.id) == m_search;
            if (!nameMatches && !idMatches) continue;
        }
        shown++;

        bool picked = this->isPicked(candidate.ref);

        auto row = CCNode::create();
        row->setContentSize({ PICKER_LIST_SIZE.width - LIST_PAD * 2, rowHeight });
        auto box = makeBox(row->getContentSize(), picked ? 140 : 70);
        if (picked) box->setColor({ 0, 120, 40 });
        row->addChildAtPosition(box, Anchor::Center);

        row->addChildAtPosition(makeLabel(name, "bigFont.fnt", 0.4f, 250.f), Anchor::Left, { 8.f, 5.f });
        auto info = makeLabel(candidate.ref.describe(), "chatFont.fnt", 0.55f, 250.f);
        info->setOpacity(170);
        row->addChildAtPosition(info, Anchor::Left, { 8.f, -8.f });

        auto menu = CCMenu::create();
        menu->setContentSize(row->getContentSize());
        menu->ignoreAnchorPointForPosition(false);
        // Checkbox: ticked = picked. The list is rebuilt after every click, so the
        // checkbox always shows the real pick state (including when picking a
        // third level unticks the oldest one).
        auto pickBox = CCMenuItemExt::createTogglerWithStandardSprites(0.6f, [this, candidate](auto) {
            this->togglePick(candidate);
        });
        pickBox->toggle(picked);
        menu->addChildAtPosition(pickBox, Anchor::Right, { -20.f, 0.f });
        row->addChildAtPosition(menu, Anchor::Center);

        content->addChild(row);
    }

    if (shown == 0) {
        char const* message =
            !m_tabLevels.empty() ? "No levels match your search." :
            m_tab == Tab::Favorites ? "No favorite levels.\nHeart a level in GD to see it here." :
            m_tab == Tab::Created ? "No created levels." :
            "No saved levels.\nPlay or download a level first.";
        auto empty = CCLabelBMFont::create(message, "bigFont.fnt");
        empty->setScale(0.4f);
        empty->setAlignment(kCCTextAlignmentCenter);
        auto holder = CCNode::create();
        holder->setContentSize({ PICKER_LIST_SIZE.width, PICKER_LIST_SIZE.height - LIST_PAD * 2 });
        holder->addChildAtPosition(empty, Anchor::Center);
        content->addChild(holder);
    }

    content->updateLayout();
    if (keepScroll) {
        // Rows are rebuilt with the same sizes, so the old offset still lines up.
        content->setPositionY(oldY);
    }
    else {
        m_list->scrollToTop();
    }
    handleTouchPriority(this);
}

void LinkPickerPopup::togglePick(Candidate const& candidate) {
    auto it = std::find_if(m_picked.begin(), m_picked.end(), [&](Candidate const& c) {
        return c.ref.sameLevel(candidate.ref);
    });
    if (it != m_picked.end()) {
        m_picked.erase(it);
    }
    else {
        // Picking a third level replaces the oldest pick.
        if (m_picked.size() == 2) m_picked.erase(m_picked.begin());
        m_picked.push_back(candidate);
    }
    // Rebuild the list next frame, not now: we're inside the checkbox's click,
    // and GD flips the checkbox after this returns. Rebuilding now would delete
    // that checkbox while GD is still using it.
    Loader::get()->queueInMainThread([self = Ref(this)] {
        self->refreshList(true);
        self->updateFooter();
    });
}

void LinkPickerPopup::updateFooter() {
    m_footerLabel->setString(fmt::format("{} of 2 picked", m_picked.size()).c_str());
    limitNodeWidth(m_footerLabel, 200.f, 0.45f, 0.1f);

    bool ready = m_picked.size() == 2;
    m_linkButton->setEnabled(ready);
    m_linkButton->setOpacity(ready ? 255 : 110);
}

void LinkPickerPopup::onLink() {
    if (m_picked.size() != 2) return;
    // Copies, because popup callbacks below run later and m_picked could change.
    Candidate first = m_picked[0];
    Candidate second = m_picked[1];

    // The Practice level is the one with startposes. -1 means "unknown"
    // (GD doesn't have the level's data), which we treat like "none".
    bool firstHas = countStartposes(first.level) > 0;
    bool secondHas = countStartposes(second.level) > 0;

    if (firstHas && !secondHas) return this->confirmLink(first, second);
    if (secondHas && !firstHas) return this->confirmLink(second, first);

    // Both or neither have startposes: ask.
    auto shortName = [](std::string name) {
        return name.size() > 14 ? name.substr(0, 13) + "." : name;
    };
    auto reason = firstHas
        ? "Both levels have startposes."
        : "Neither level has startposes.";
    createQuickPopup(
        "Which is the Practice level?",
        fmt::format("{} The Practice level is the copy you use startposes in.\n\n"
                    "<cy>{}</c> or <cy>{}</c>?",
                    reason, std::string(first.level->m_levelName), std::string(second.level->m_levelName)),
        shortName(first.level->m_levelName).c_str(),
        shortName(second.level->m_levelName).c_str(),
        [this, first, second](auto, bool pickedSecond) {
            if (pickedSecond) this->confirmLink(second, first);
            else this->confirmLink(first, second);
        }
    );
}

// If either level is already linked, say which links would be replaced and
// only continue if you agree. Nothing is deleted without asking.
void LinkPickerPopup::confirmLink(Candidate const& practice, Candidate const& target) {
    auto conflicts = conflictingLinks({ practice.ref, target.ref });
    if (conflicts.empty()) return this->finishLink(practice, target);

    std::string list;
    for (auto const& link : conflicts) {
        bool found;
        list += fmt::format("\n<cy>{}</c> + <cg>{}</c>",
            displayName(link.practice, found), displayName(link.target, found));
    }
    createQuickPopup(
        "Replace link?",
        fmt::format("A level can only be in one link. This will replace:{}", list),
        "Cancel", "Replace",
        [this, practice, target](auto, bool confirmed) {
            if (confirmed) this->finishLink(practice, target);
        }
    );
}

void LinkPickerPopup::finishLink(Candidate const& practice, Candidate const& target) {
    addLink({ practice.ref, target.ref });
    Notification::create(
        fmt::format("Linked {}\nto {}", std::string(practice.level->m_levelName), std::string(target.level->m_levelName)),
        NotificationIcon::Success
    )->show();
    if (m_parent) m_parent->refreshList();
    this->onClose(nullptr);
}
