#pragma once
// A Link pairs a Practice level (the copy with startposes) with a Target level
// (the one you're trying to beat). Links are saved in the mod's save data.

#include "LevelRef.hpp"

struct Link {
    LevelRef practice;
    LevelRef target;
};

std::vector<Link> loadLinks();
void saveLinks(std::vector<Link> const& links);

// Existing links that share a level with this one. A level can only be in one
// link (so swapping always has one clear destination), so these would be
// replaced by addLink. The UI asks before that happens.
std::vector<Link> conflictingLinks(Link const& link);

// Adds a link, replacing any conflicting ones (see above).
void addLink(Link const& link);
void removeLink(size_t index);

// The link this level belongs to, if any.
std::optional<Link> findLinkFor(GJGameLevel* level);
