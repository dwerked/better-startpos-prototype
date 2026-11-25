#pragma once
// Fast SP: prepare every startpos's saved state before the level
// starts, so every switch afterwards is near-instant (see swapStartCheckpoint in
// StartposSwitcher.cpp for what a "saved state" is).
//
// Turned on with the "Fast SP" checkbox under the Play button on a level's
// page (then press Play), or by default for every level
// with the "fast-sp-auto" setting. The preparing itself happens in
// StartposSwitcher.cpp (it needs the level's startpos list); this file holds
// the level-page checkbox and its state.

#include <Geode/Geode.hpp>

// Called when a level starts. True if this level should prepare all startposes:
// its checkbox is ticked (remembered per level for this session), or, if you
// never touched its checkbox, the "on every Play" setting is on.
bool takeFastSPRequest(GJGameLevel* level);
