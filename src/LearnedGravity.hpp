#pragma once
// Learned gravity: the gravity you actually had when you passed a startpos.
//
// Smart startpos can't tell whether gravity orbs and pads flip you (that
// depends on your clicks). So when you play past such a startpos, the
// switcher writes down whether you were upside down at that moment, and from
// then on that startpos starts you that way. Saved per level, by the
// startpos's x position, in the mod's save data.

#include <Geode/Geode.hpp>

// What was learned for the startpos at `x` in this level, if anything.
std::optional<bool> getLearnedGravity(GJGameLevel* level, float x);

// Remembers it. Returns true if this was new or different (and was saved).
bool setLearnedGravity(GJGameLevel* level, float x, bool upsideDown);
