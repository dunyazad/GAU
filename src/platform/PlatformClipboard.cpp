#include "PlatformClipboard.h"

#include <SDL3/SDL.h>

std::string GetClipboardText()
{
    char* text = SDL_GetClipboardText();
    if (text == nullptr) {
        return std::string();
    }
    std::string result(text);
    SDL_free(text);
    return result;
}

void SetClipboardText(const std::string& text)
{
    SDL_SetClipboardText(text.c_str());
}
