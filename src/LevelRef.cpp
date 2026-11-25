#include "LevelRef.hpp"

using namespace geode::prelude;

LevelRef LevelRef::fromLevel(GJGameLevel* level) {
    LevelRef ref;
    ref.isLocal = level->m_levelType == GJLevelType::Editor;
    ref.id = ref.isLocal ? 0 : level->m_levelID.value();
    ref.name = level->m_levelName;
    ref.originalID = level->m_originalLevel.value();
    return ref;
}

GJGameLevel* LevelRef::resolve() const {
    if (!isLocal) {
        return GameLevelManager::get()->getSavedLevel(id);
    }

    // Local: prefer a level with the same name AND the same source level.
    // Fall back to name only, in case the copy has no source ID.
    GJGameLevel* nameOnlyMatch = nullptr;
    for (auto level : CCArrayExt<GJGameLevel*>(LocalLevelManager::get()->m_localLevels)) {
        if (level->m_levelName != name) continue;
        if (level->m_originalLevel.value() == originalID) return level;
        if (!nameOnlyMatch) nameOnlyMatch = level;
    }
    return nameOnlyMatch;
}

bool LevelRef::sameLevel(LevelRef const& other) const {
    if (isLocal != other.isLocal) return false;
    if (!isLocal) return id == other.id;
    return name == other.name && originalID == other.originalID;
}

std::string LevelRef::describe() const {
    if (isLocal) return "Local level";
    return fmt::format("Online, ID {}", id);
}

matjson::Value LevelRef::toJson() const {
    auto json = matjson::Value::object();
    json["local"] = isLocal;
    json["id"] = id;
    json["name"] = name;
    json["original"] = originalID;
    return json;
}

std::optional<LevelRef> LevelRef::fromJson(matjson::Value const& json) {
    if (!json.isObject()) return std::nullopt;
    LevelRef ref;
    ref.isLocal = json["local"].asBool().unwrapOr(false);
    ref.id = static_cast<int>(json["id"].asInt().unwrapOr(0));
    ref.name = json["name"].asString().unwrapOr("");
    ref.originalID = static_cast<int>(json["original"].asInt().unwrapOr(0));
    // A ref with nothing to find the level by is useless; drop it.
    if (!ref.isLocal && ref.id == 0) return std::nullopt;
    if (ref.isLocal && ref.name.empty()) return std::nullopt;
    return ref;
}

int countStartposes(GJGameLevel* level) {
    if (!level) return -1;
    std::string data = level->m_levelString;
    if (data.empty()) return -1;

    // Saved level data is compressed. Uncompressed data starts with "k" or similar
    // header text; compressed data is base64 gzip, which starts with "H4sI".
    if (data.starts_with("H4sI")) {
        data = ZipUtils::decompressString(data, false, 0);
        if (data.empty()) return -1;
    }

    // Objects are separated by ';'. Each object is "key,value,key,value,..."
    // and key "1" is the object ID. Start positions have ID 31.
    // The first chunk is the level header, not an object, so skip it.
    int count = 0;
    size_t start = data.find(';');
    while (start != std::string::npos) {
        size_t end = data.find(';', start + 1);
        std::string_view object(data.data() + start + 1,
            (end == std::string::npos ? data.size() : end) - start - 1);
        if (object.starts_with("1,31,") || object == "1,31") count++;
        start = end;
    }
    return count;
}
