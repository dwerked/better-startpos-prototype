#include "LearnedGravity.hpp"
#include "LevelRef.hpp"

using namespace geode::prelude;

// Stored as a list of { level, x, upsideDown } entries under "learned-gravity".
// Startposes are matched by x position; anything within half a unit counts
// as the same startpos (positions are stored as decimals).
static constexpr float SAME_X = 0.5f;
static constexpr auto KEY = "learned-gravity";

static bool sameEntry(matjson::Value const& entry, LevelRef const& ref, float x) {
    auto entryRef = LevelRef::fromJson(entry["level"]);
    if (!entryRef || !entryRef->sameLevel(ref)) return false;
    auto entryX = entry["x"].asDouble();
    return entryX && std::abs(static_cast<float>(*entryX) - x) < SAME_X;
}

std::optional<bool> getLearnedGravity(GJGameLevel* level, float x) {
    if (!level) return std::nullopt;
    auto ref = LevelRef::fromLevel(level);
    auto list = Mod::get()->getSavedValue<matjson::Value>(KEY);
    if (!list.isArray()) return std::nullopt;
    for (auto const& entry : list) {
        if (sameEntry(entry, ref, x)) return entry["upsideDown"].asBool().ok();
    }
    return std::nullopt;
}

bool setLearnedGravity(GJGameLevel* level, float x, bool upsideDown) {
    if (!level) return false;
    if (getLearnedGravity(level, x) == upsideDown) return false; // nothing new: don't write

    auto ref = LevelRef::fromLevel(level);
    auto old = Mod::get()->getSavedValue<matjson::Value>(KEY);
    auto list = matjson::Value::array();
    if (old.isArray()) {
        for (auto const& entry : old) {
            if (!sameEntry(entry, ref, x)) list.push(entry);
        }
    }
    auto entry = matjson::Value::object();
    entry["level"] = ref.toJson();
    entry["x"] = x;
    entry["upsideDown"] = upsideDown;
    list.push(entry);
    Mod::get()->setSavedValue(KEY, list);

    // Write immediately, so a crash can't lose it (see saveLinks).
    if (auto result = Mod::get()->saveData(); !result) {
        log::warn("Couldn't save learned gravity: {}", result.unwrapErr());
    }
    return true;
}
