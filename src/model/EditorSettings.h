#pragma once

#include <string>
#include <vector>

// Persistent editor preferences, stored as JSON in the working directory
// (editor_settings.json). Loaded at startup, saved on exit. To persist a
// new piece of state: add a member here and extend LoadFromFile/SaveToFile.
// One remembered open document: its file and the canvas view it had.
// zoom == 0 means "no saved view" (use the default zoom). Untitled
// documents are backed by an auto-managed session file: path points at
// that file, untitled stays true and displayName keeps the tab name.
struct OpenFileEntry
{
    std::string path;
    bool untitled = false;
    std::string displayName;
    float panX = 0.0f;
    float panY = 0.0f;
    float zoom = 0.0f;
};

struct EditorSettings
{
    // Names of context-menu categories the user collapsed.
    std::vector<std::string> collapsedCategories;

    // Graph files that were open when the editor exited; reopened on
    // startup with their canvas views (untitled documents are not
    // persisted).
    std::vector<OpenFileEntry> openFiles;
    // Path of the document that was active, empty if it was untitled.
    std::string activeFilePath;

    // Returns false when the file is missing or unreadable; the settings
    // keep their defaults in that case. Unknown JSON entries are ignored.
    bool LoadFromFile(const std::string& path);

    bool SaveToFile(const std::string& path) const;
};
