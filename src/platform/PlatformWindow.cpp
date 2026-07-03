#include "PlatformWindow.h"
#include "GLLoader.h"
#include "PlatformInput.h"

#include <SDL3/SDL.h>

PlatformWindow::~PlatformWindow()
{
    Shutdown();
}

bool PlatformWindow::Init(const char* title, int width, int height)
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("PlatformWindow: SDL_Init failed: %s", SDL_GetError());
        return false;
    }
    sdlInitialized = true;

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window = SDL_CreateWindow(
        title, width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        SDL_Log("PlatformWindow: SDL_CreateWindow failed: %s", SDL_GetError());
        Shutdown();
        return false;
    }

    glContext = SDL_GL_CreateContext(window);
    if (glContext == nullptr) {
        SDL_Log("PlatformWindow: SDL_GL_CreateContext failed: %s", SDL_GetError());
        Shutdown();
        return false;
    }

    if (!SDL_GL_MakeCurrent(window, glContext)) {
        SDL_Log("PlatformWindow: SDL_GL_MakeCurrent failed: %s", SDL_GetError());
        Shutdown();
        return false;
    }

    if (!SDL_GL_SetSwapInterval(1)) {
        SDL_Log("PlatformWindow: vsync unavailable: %s", SDL_GetError());
    }

    if (!LoadGLFunctions()) {
        SDL_Log("PlatformWindow: failed to load required GL functions");
        Shutdown();
        return false;
    }

    // Text input feeds the node-search menu; keep it enabled on desktop.
    if (!SDL_StartTextInput(window)) {
        SDL_Log("PlatformWindow: SDL_StartTextInput failed: %s", SDL_GetError());
    }

    UpdateSizes();
    return true;
}

void PlatformWindow::Shutdown()
{
    if (glContext != nullptr) {
        SDL_GL_DestroyContext(glContext);
        glContext = nullptr;
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    if (sdlInitialized) {
        SDL_Quit();
        sdlInitialized = false;
    }
}

bool PlatformWindow::PumpEvents(std::vector<EditorInputEvent>& outEvents)
{
    outEvents.clear();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT:
            return false;
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
            return false;
        case SDL_EVENT_WINDOW_RESIZED:
        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            UpdateSizes();
            break;
        default: {
            EditorInputEvent inputEvent;
            if (TranslateSDLEvent(event, inputEvent)) {
                outEvents.push_back(inputEvent);
            }
            break;
        }
        }
    }
    return true;
}

void PlatformWindow::BeginFrame(float clearR, float clearG, float clearB)
{
    glViewport(0, 0, drawableWidth, drawableHeight);
    glClearColor(clearR, clearG, clearB, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
}

void PlatformWindow::EndFrame()
{
    SDL_GL_SwapWindow(window);
}

float PlatformWindow::GetPixelRatio() const
{
    if (windowWidth <= 0) {
        return 1.0f;
    }
    return static_cast<float>(drawableWidth) / static_cast<float>(windowWidth);
}

void PlatformWindow::UpdateSizes()
{
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);
    SDL_GetWindowSizeInPixels(window, &drawableWidth, &drawableHeight);
}

bool PlatformWindow::IsMaximized() const
{
    if (window == nullptr) {
        return false;
    }
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_MAXIMIZED) != 0;
}

void PlatformWindow::Maximize()
{
    if (window != nullptr) {
        SDL_MaximizeWindow(window);
        UpdateSizes();
    }
}
