#include "Links.hpp"

using namespace geode::prelude;

std::vector<Link> loadLinks() {
    std::vector<Link> links;
    auto saved = Mod::get()->getSavedValue<matjson::Value>("links");
    if (!saved.isArray()) return links;
    for (auto const& entry : saved) {
        auto practice = LevelRef::fromJson(entry["practice"]);
        auto target = LevelRef::fromJson(entry["target"]);
        if (practice && target) links.push_back({ *practice, *target });
    }
    return links;
}

void saveLinks(std::vector<Link> const& links) {
    auto list = matjson::Value::array();
    for (auto const& link : links) {
        auto entry = matjson::Value::object();
        entry["practice"] = link.practice.toJson();
        entry["target"] = link.target.toJson();
        list.push(entry);
    }
    Mod::get()->setSavedValue("links", list);
    // Geode normally writes mod data to disk only when GD closes cleanly, so a
    // crash would lose links made this session. Write it now instead.
    if (auto result = Mod::get()->saveData(); !result) {
        log::warn("Couldn't save links: {}", result.unwrapErr());
    }
}

static bool usesLevel(Link const& link, LevelRef const& ref) {
    return link.practice.sameLevel(ref) || link.target.sameLevel(ref);
}

std::vector<Link> conflictingLinks(Link const& link) {
    std::vector<Link> result;
    for (auto const& existing : loadLinks()) {
        if (usesLevel(existing, link.practice) || usesLevel(existing, link.target)) {
            result.push_back(existing);
        }
    }
    return result;
}

void addLink(Link const& link) {
    auto links = loadLinks();
    std::erase_if(links, [&](Link const& existing) {
        return usesLevel(existing, link.practice) || usesLevel(existing, link.target);
    });
    links.insert(links.begin(), link); // newest first
    saveLinks(links);
}

void removeLink(size_t index) {
    auto links = loadLinks();
    if (index >= links.size()) return;
    links.erase(links.begin() + index);
    saveLinks(links);
}

std::optional<Link> findLinkFor(GJGameLevel* level) {
    if (!level) return std::nullopt;
    auto ref = LevelRef::fromLevel(level);
    for (auto const& link : loadLinks()) {
        if (usesLevel(link, ref)) return link;
    }
    return std::nullopt;
}
