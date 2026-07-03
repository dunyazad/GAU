#include "PlatformFileDialog.h"

#include <SDL3/SDL.h>

#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

static std::mutex resultMutex;
static std::vector<FileDialogResult> pendingResults;

static const SDL_DialogFileFilter GRAPH_FILE_FILTERS[] = {
    {"Graph JSON", "json"},
};

// May be invoked from a non-main thread (per SDL docs); only touches
// the mutex-guarded queue.
static void FileDialogCallback(void* userdata, const char* const* fileList, int filterIndex)
{
    (void)filterIndex;

    FileDialogResult result;
    result.type = static_cast<FileDialogType>(reinterpret_cast<std::intptr_t>(userdata));
    if (fileList != nullptr && fileList[0] != nullptr) {
        result.accepted = true;
        result.path = fileList[0];
    }

    std::lock_guard<std::mutex> lock(resultMutex);
    pendingResults.push_back(std::move(result));
}

void ShowGraphFileDialog(SDL_Window* window, FileDialogType type)
{
    void* userdata = reinterpret_cast<void*>(static_cast<std::intptr_t>(static_cast<int>(type)));
    if (type == FileDialogType::OpenGraph) {
        SDL_ShowOpenFileDialog(FileDialogCallback, userdata, window,
                               GRAPH_FILE_FILTERS, 1, nullptr, false);
    } else {
        SDL_ShowSaveFileDialog(FileDialogCallback, userdata, window,
                               GRAPH_FILE_FILTERS, 1, nullptr);
    }
}

bool PollFileDialogResult(FileDialogResult& outResult)
{
    std::lock_guard<std::mutex> lock(resultMutex);
    if (pendingResults.empty()) {
        return false;
    }
    outResult = std::move(pendingResults.front());
    pendingResults.erase(pendingResults.begin());
    return true;
}

ConfirmSaveResult ShowConfirmSaveDialog(SDL_Window* window, const std::string& documentName)
{
    const SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Save"},
        {0, 1, "Don't Save"},
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 2, "Cancel"},
    };

    const std::string message = "Save changes to \"" + documentName + "\" before closing?";

    SDL_MessageBoxData data = {};
    data.flags = SDL_MESSAGEBOX_WARNING;
    data.window = window;
    data.title = "Unsaved Changes";
    data.message = message.c_str();
    data.numbuttons = 3;
    data.buttons = buttons;

    int buttonId = 2;
    if (!SDL_ShowMessageBox(&data, &buttonId)) {
        SDL_Log("PlatformFileDialog: SDL_ShowMessageBox failed: %s", SDL_GetError());
        return ConfirmSaveResult::Cancel;
    }
    switch (buttonId) {
    case 0:
        return ConfirmSaveResult::Save;
    case 1:
        return ConfirmSaveResult::Discard;
    default:
        return ConfirmSaveResult::Cancel;
    }
}
