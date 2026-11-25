#pragma once
// Smart startpos.
//
// A startpos says how the player starts there: speed, gamemode, mini or not,
// gravity, dual, mirrored. Creators often place startposes without setting
// these, so you start as a cube at normal speed in the middle of a 3x ship
// part. Smart startpos works out the right settings from the level itself:
//
//  1. Start from the level's own start settings (how you'd begin from 0).
//  2. Walk through the level's portals from left to right, changing the
//     settings the way each portal would (ship portal = ship, and so on).
//  3. Every startpos gets the settings reached at its position.
//
// Like Mega Hack's, this only looks at left-to-right order: it can't know
// whether the player actually flies through a portal (a portal above or
// below the path still counts). The changes are made to the startposes in
// memory only; the level file is never touched.
//
// Gravity orbs and pads (blue, green, spider...) are a harder case: whether
// they flip you depends on whether you click them, which no list of objects
// can tell. So a startpos with one of those before it is marked "gravity
// unsure", and the switcher learns its gravity from your own runs instead
// (see "Learned gravity" in StartposSwitcher.cpp).

#include <Geode/Geode.hpp>

// The player settings a startpos holds, all in one place.
struct PlayerState {
    int gamemode = 0; // 0 cube, 1 ship, 2 ball, 3 UFO, 4 wave, 5 robot, 6 spider, 7 swing
    Speed speed = Speed::Normal;
    bool mini = false;
    bool upsideDown = false;
    bool dual = false;
    bool mirrored = false;

    static PlayerState from(LevelSettingsObject* settings);
    // Copies this state into a startpos's settings. Returns true if anything changed.
    bool writeTo(LevelSettingsObject* settings) const;
    // What going through this portal does.
    void pass(GameObject* portal);

    bool operator==(PlayerState const&) const = default;
};

// Is this one of the objects smart startpos follows (portals, plus gravity
// orbs and pads)?
bool isSmartPortal(GameObject* object);

struct SmartResult {
    int changed = 0;                  // startposes with at least one setting changed
    std::vector<bool> gravityUnsure;  // per startpos: a gravity orb/pad could have flipped you
};

// Gives each startpos the settings the portals before it lead to.
// `startposes` must be sorted left to right; `portals` can be in any order.
SmartResult applySmartStartpos(LevelSettingsObject* levelStart,
                               std::vector<GameObject*> portals,
                               std::vector<StartPosObject*> const& startposes);
