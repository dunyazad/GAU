#pragma once

#include <string>

struct SDL_Window;

enum class FileDialogType
{
    OpenGraph,
    SaveGraph,
};

struct FileDialogResult
{
    FileDialogType type = FileDialogType::OpenGraph;
    // False when the user cancelled the dialog.
    bool accepted = false;
    std::string path;
};

// Shows the native open/save dialog asynchronously (SDL3). The result
// callback may run on another thread, so results are queued internally;
// poll them from the main loop with PollFileDialogResult.
void ShowGraphFileDialog(SDL_Window* window, FileDialogType type);

// Pops one completed dialog result. Returns false when none is pending.
bool PollFileDialogResult(FileDialogResult& outResult);
