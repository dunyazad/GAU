#pragma once

#include "interaction/EditorInputEvent.h"

#include <vector>

struct SDL_Window;
struct SDL_GLContextState;

// Owns the SDL window and GL context. The only place (besides other
// platform/ files) that talks to SDL directly.
class PlatformWindow
{
public:
    PlatformWindow() = default;
    ~PlatformWindow();

    PlatformWindow(const PlatformWindow&) = delete;
    PlatformWindow& operator=(const PlatformWindow&) = delete;

    // Creates the SDL window and a GL 3.3 core context, loads GL functions.
    // Returns false on failure; errors are logged via SDL_Log.
    bool Init(const char* title, int width, int height);

    // Destroys context/window and shuts SDL down. Safe to call twice.
    void Shutdown();

    // Processes pending SDL events. Window-level events (resize, quit) are
    // handled internally; input events are normalized and appended to
    // outEvents (cleared first). Returns false when quit was requested.
    bool PumpEvents(std::vector<EditorInputEvent>& outEvents);

    // Sets the viewport to the drawable size and clears the framebuffer.
    void BeginFrame(float clearR, float clearG, float clearB);

    // Presents the frame.
    void EndFrame();

    // Logical window size (points).
    int GetWidth() const { return windowWidth; }
    int GetHeight() const { return windowHeight; }

    // Framebuffer size (pixels), differs from logical size on HiDPI.
    int GetDrawableWidth() const { return drawableWidth; }
    int GetDrawableHeight() const { return drawableHeight; }

    float GetPixelRatio() const;

    // Opaque handle for other platform modules (e.g. file dialogs).
    SDL_Window* GetSDLWindow() const { return window; }

private:
    void UpdateSizes();

    SDL_Window* window = nullptr;
    SDL_GLContextState* glContext = nullptr;
    int windowWidth = 0;
    int windowHeight = 0;
    int drawableWidth = 0;
    int drawableHeight = 0;
    bool sdlInitialized = false;
};
