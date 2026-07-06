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
