#include "PlatformNVG.h"
#include "GLLoader.h"

#include <SDL3/SDL.h>

#include <nanovg.h>

#define NANOVG_GL3_IMPLEMENTATION
#include <nanovg_gl.h>

// Registers "sans" and "sans-bold" from system fonts. If no bold face is
// found, "sans-bold" aliases the regular file so text always renders.
static void LoadDefaultFonts(NVGcontext* vg)
{
#if defined(_WIN32)
    const char* regularCandidates[] = {
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/arial.ttf",
    };
    const char* boldCandidates[] = {
        "C:/Windows/Fonts/segoeuib.ttf",
        "C:/Windows/Fonts/arialbd.ttf",
    };
#elif defined(__APPLE__)
    const char* regularCandidates[] = {
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/System/Library/Fonts/Helvetica.ttc",
    };
    const char* boldCandidates[] = {
        "/System/Library/Fonts/Supplemental/Arial Bold.ttf",
    };
#else
    const char* regularCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    };
    const char* boldCandidates[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    };
#endif

    const char* loadedRegularPath = nullptr;
    for (const char* path : regularCandidates) {
        if (nvgCreateFont(vg, "sans", path) >= 0) {
            loadedRegularPath = path;
            break;
        }
    }
    if (loadedRegularPath == nullptr) {
        SDL_Log("PlatformNVG: no system font found; text will not render");
        return;
    }

    for (const char* path : boldCandidates) {
        if (nvgCreateFont(vg, "sans-bold", path) >= 0) {
            return;
        }
    }
    nvgCreateFont(vg, "sans-bold", loadedRegularPath);
}

NVGcontext* CreatePlatformNVGContext()
{
    NVGcontext* vg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    if (vg == nullptr) {
        return nullptr;
    }
    LoadDefaultFonts(vg);
    return vg;
}

void DestroyPlatformNVGContext(NVGcontext* vg)
{
    if (vg != nullptr) {
        nvgDeleteGL3(vg);
    }
}
