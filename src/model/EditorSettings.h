#pragma once

#include "GraphTypes.h"

#include <string>

// Persistent editor preferences, stored as JSON in the working directory
// (editor_settings.json). Loaded at startup, saved on exit. To persist a
// new piece of state: add a member here and extend LoadFromFile/SaveToFile.
struct EditorSettings
{
    // Context menu category collapse state, indexed by NodeCategoryIndex().
    bool categoryCollapsed[NODE_CATEGORY_COUNT] = {};

    // Returns false when the file is missing or unreadable; the settings
    // keep their defaults in that case. Unknown JSON entries are ignored.
    bool LoadFromFile(const std::string& path);

    bool SaveToFile(const std::string& path) const;
};
