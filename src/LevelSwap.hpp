#pragma once
// Swapping between linked levels.
//
// GD can only run one level at a time, so a swap is:
//   1. quit the current level the normal way (GD saves attempts, stops music)
//   2. wait until GD has completely left that level
//   3. open the other level
// Opening the new level before the old one is gone would leave two levels
// fighting over GD's "current level" and crash, so step 2 matters.

#include <Geode/Geode.hpp>

// Where the level we swap to should begin. The other level's startposes aren't
// known until it loads, so positions are counted from either end:
//   {FromFirst, 0} = first startpos, {FromFirst, 1} = second, ...
//   {FromLast, 0}  = last startpos,  {FromLast, 1}  = second to last, ...
struct SwapStart {
    enum class From { Zero, First, Last };
    From from = From::Zero; // Zero = open normally (used for the Target level)
    int offset = 0;

    static SwapStart fromZero() { return {}; }
    static SwapStart fromFirst(int offset) { return { From::First, offset }; }
    static SwapStart fromLast(int offset) { return { From::Last, offset }; }
};

// Starts a swap from the level that's playing now to `level`.
// Does nothing if a swap is already in progress.
// parkCurrent: keep the current level loaded off screen (Practice -> Target),
// if the "keep-practice-loaded" setting allows it, so swapping back is instant.
void swapToLevel(PlayLayer* current, GJGameLevel* level, SwapStart start, bool parkCurrent);

// After a parked level is shown again, the switcher asks where to start it.
// Returns the start (once) if `level` is the parked level coming back.
std::optional<SwapStart> takeParkedResume(PlayLayer* level);

// True from the moment a swap starts until the new level is shown. While this
// is true, switching and pausing are blocked: in seamless mode GD's "current
// level" is already the new, not-yet-visible level.
bool isSwapInProgress();

// Seamless mode: the old level's quit code stops all music, including the new
// level's song, which was already loaded. The switcher calls this when a level
// starts; it returns true (once) if that level needs its music restarted.
bool takeMusicRestart(PlayLayer* level);

// When a level loads, the switcher asks whether a swap wants it to begin at a
// particular startpos. Returns fromZero() if there's nothing pending for it.
SwapStart takePendingStart(GJGameLevel* level);

// Throws away the parked Practice level, if there is one (frees its memory).
// Used when "Keep Practice loaded" or "Seamless swap" is turned off, so the
// change applies right away instead of at the next swap. Never the level on
// screen: a parked level is by definition not showing.
void dropParkedLevel();
