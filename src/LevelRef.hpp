#pragma once
// A LevelRef is how we find a level again later, even after restarting GD.
//
// Online levels (downloaded/rated) have a permanent ID, so that's all we need.
// Local levels (ones you created or copied) have NO permanent ID in GD's save
// file, so we remember the level's name plus the ID of the online level it was
// copied from (GD stores that on every copy). If you rename a local level,
// its links can't find it anymore and the Links screen says so.

#include <Geode/Geode.hpp>

struct LevelRef {
    bool isLocal = false;
    int id = 0;          // online levels: the level ID
    std::string name;    // local levels: used to find it; online: shown in UI
    int originalID = 0;  // local copies: the online level it was copied from

    static LevelRef fromLevel(GJGameLevel* level);

    // Finds the actual level in GD's data. nullptr if it no longer exists.
    GJGameLevel* resolve() const;

    // Two refs point at the same level.
    bool sameLevel(LevelRef const& other) const;

    // Short description for the UI, e.g. "Local level" or "Online, ID 123".
    std::string describe() const;

    matjson::Value toJson() const;
    static std::optional<LevelRef> fromJson(matjson::Value const& json);
};

// Number of start positions in a level's data, without loading the level.
// Returns -1 if GD doesn't have the level's data (e.g. never downloaded).
int countStartposes(GJGameLevel* level);
