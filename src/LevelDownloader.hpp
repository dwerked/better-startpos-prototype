#pragma once
// GD doesn't keep online levels' data between sessions: the level stays in
// your Saved list, but its objects are re-downloaded when you open its page.
// So the first time you swap to a linked online level in a session, GD may not
// have its data yet. When you enter a linked level, we download the other one
// in the background (the same request GD's level page makes), so it's ready by
// the time you swap.

#include <Geode/Geode.hpp>

// True if GD has this level's data (local levels always do).
bool hasLevelData(GJGameLevel* level);

// Starts a background download if the level has no data. Safe to call often:
// it does nothing if the data is there or a download is already running.
void prefetchLevelData(GJGameLevel* level);

// True while a background download for this level is running.
bool isDownloadingLevel(GJGameLevel* level);
