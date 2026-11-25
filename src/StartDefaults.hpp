#pragma once
// The settings for how a level starts, from Settings:
//  - Default startpos: which startpos a level begins at when you open it.
//  - Smart startpos: give every startpos the speed, gamemode, etc. that the
//    level's portals say it should have (see SmartStartpos.cpp).
// One set for all levels, plus an optional override per level (the override
// wins, and covers both). Saved in the mod's save data.

#include <Geode/Geode.hpp>
#include "LevelRef.hpp"

struct StartDefault {
    enum class Mode { Last, First, Start, Number };
    Mode mode = Mode::Last; // Last = what GD does on its own
    int number = 1;         // used when mode == Number (1 = first startpos)
    bool smart = true;      // smart startpos on (on by default)

    std::string describe() const; // "Last", "First", "From 0", "Number"
    matjson::Value toJson() const;
    static StartDefault fromJson(matjson::Value const& json);

    // The startpos index (0 = start of level, 1..count) this default picks.
    int pickIndex(int count) const;
};

StartDefault getGlobalDefault();
void setGlobalDefault(StartDefault const& value);

// nullopt = this level follows the all-levels setting.
std::optional<StartDefault> getLevelDefault(GJGameLevel* level);
void setLevelDefault(GJGameLevel* level, std::optional<StartDefault> const& value);

// The default that applies to this level (its override, else the global one).
StartDefault effectiveDefault(GJGameLevel* level);
