#include "StartDefaults.hpp"

using namespace geode::prelude;

std::string StartDefault::describe() const {
    switch (mode) {
        case Mode::First: return "First";
        case Mode::Start: return "From 0";
        case Mode::Number: return "Number";
        default: return "Last";
    }
}

matjson::Value StartDefault::toJson() const {
    auto json = matjson::Value::object();
    json["mode"] = static_cast<int>(mode);
    json["number"] = number;
    json["smart"] = smart;
    return json;
}

StartDefault StartDefault::fromJson(matjson::Value const& json) {
    StartDefault value;
    auto mode = json["mode"].asInt().unwrapOr(0);
    if (mode >= 0 && mode <= 3) value.mode = static_cast<Mode>(mode);
    value.number = std::max<int>(1, static_cast<int>(json["number"].asInt().unwrapOr(1)));
    // Saved before smart startpos existed = not set yet = on (the default).
    value.smart = json["smart"].asBool().unwrapOr(true);
    return value;
}

int StartDefault::pickIndex(int count) const {
    if (count <= 0) return 0;
    switch (mode) {
        case Mode::First: return 1;
        case Mode::Start: return 0;
        case Mode::Number: return std::clamp(number, 1, count); // too high = the last one
        default: return count;
    }
}

static void saveNow() {
    // Write immediately, so a crash can't lose it (see saveLinks).
    if (auto result = Mod::get()->saveData(); !result) {
        log::warn("Couldn't save settings: {}", result.unwrapErr());
    }
}

StartDefault getGlobalDefault() {
    return StartDefault::fromJson(Mod::get()->getSavedValue<matjson::Value>("default-start"));
}

void setGlobalDefault(StartDefault const& value) {
    Mod::get()->setSavedValue("default-start", value.toJson());
    saveNow();
}

// Per-level overrides are stored as a list of { level, default } pairs.
std::optional<StartDefault> getLevelDefault(GJGameLevel* level) {
    if (!level) return std::nullopt;
    auto ref = LevelRef::fromLevel(level);
    auto list = Mod::get()->getSavedValue<matjson::Value>("level-defaults");
    if (!list.isArray()) return std::nullopt;
    for (auto const& entry : list) {
        auto entryRef = LevelRef::fromJson(entry["level"]);
        if (entryRef && entryRef->sameLevel(ref)) return StartDefault::fromJson(entry["default"]);
    }
    return std::nullopt;
}

void setLevelDefault(GJGameLevel* level, std::optional<StartDefault> const& value) {
    if (!level) return;
    auto ref = LevelRef::fromLevel(level);
    auto old = Mod::get()->getSavedValue<matjson::Value>("level-defaults");
    auto list = matjson::Value::array();
    if (old.isArray()) {
        for (auto const& entry : old) {
            auto entryRef = LevelRef::fromJson(entry["level"]);
            if (entryRef && !entryRef->sameLevel(ref)) list.push(entry);
        }
    }
    if (value) {
        auto entry = matjson::Value::object();
        entry["level"] = ref.toJson();
        entry["default"] = value->toJson();
        list.push(entry);
    }
    Mod::get()->setSavedValue("level-defaults", list);
    saveNow();
}

StartDefault effectiveDefault(GJGameLevel* level) {
    return getLevelDefault(level).value_or(getGlobalDefault());
}
