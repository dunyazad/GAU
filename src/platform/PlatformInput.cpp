#include "PlatformInput.h"

#include <SDL3/SDL.h>

#include <cstddef>

static EditorKey TranslateKey(SDL_Keycode keycode, SDL_Keymod mod)
{
    const bool ctrl = (mod & SDL_KMOD_CTRL) != 0;
    const bool shift = (mod & SDL_KMOD_SHIFT) != 0;

    switch (keycode) {
    case SDLK_BACKSPACE:
        return EditorKey::Backspace;
    case SDLK_ESCAPE:
        return EditorKey::Escape;
    case SDLK_RETURN:
    case SDLK_KP_ENTER:
        return EditorKey::Enter;
    case SDLK_TAB:
        return EditorKey::Tab;
    case SDLK_Z:
        if (ctrl) {
            return shift ? EditorKey::Redo : EditorKey::Undo;
        }
        return EditorKey::None;
    case SDLK_Y:
        return ctrl ? EditorKey::Redo : EditorKey::None;
    default:
        return EditorKey::None;
    }
}

static EditorMouseButton TranslateMouseButton(Uint8 sdlButton)
{
    switch (sdlButton) {
    case SDL_BUTTON_LEFT:
        return EditorMouseButton::Left;
    case SDL_BUTTON_RIGHT:
        return EditorMouseButton::Right;
    case SDL_BUTTON_MIDDLE:
        return EditorMouseButton::Middle;
    default:
        return EditorMouseButton::None;
    }
}

bool TranslateSDLEvent(const SDL_Event& sdlEvent, EditorInputEvent& outEvent)
{
    switch (sdlEvent.type) {
    case SDL_EVENT_MOUSE_MOTION:
        outEvent.type = EditorInputType::MouseMove;
        outEvent.button = EditorMouseButton::None;
        outEvent.x = sdlEvent.motion.x;
        outEvent.y = sdlEvent.motion.y;
        outEvent.wheelDelta = 0.0f;
        return true;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
        const EditorMouseButton button = TranslateMouseButton(sdlEvent.button.button);
        if (button == EditorMouseButton::None) {
            return false;
        }
        outEvent.type = (sdlEvent.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
                            ? EditorInputType::MouseDown
                            : EditorInputType::MouseUp;
        outEvent.button = button;
        outEvent.x = sdlEvent.button.x;
        outEvent.y = sdlEvent.button.y;
        outEvent.alt = (SDL_GetModState() & SDL_KMOD_ALT) != 0;
        outEvent.ctrl = (SDL_GetModState() & SDL_KMOD_CTRL) != 0;
        outEvent.wheelDelta = 0.0f;
        return true;
    }

    case SDL_EVENT_MOUSE_WHEEL:
        outEvent.type = EditorInputType::MouseWheel;
        outEvent.button = EditorMouseButton::None;
        outEvent.x = sdlEvent.wheel.mouse_x;
        outEvent.y = sdlEvent.wheel.mouse_y;
        outEvent.wheelDelta = sdlEvent.wheel.y;
        return true;

    case SDL_EVENT_KEY_DOWN: {
        const EditorKey key = TranslateKey(sdlEvent.key.key, sdlEvent.key.mod);
        if (key == EditorKey::None) {
            return false;
        }
        outEvent.type = EditorInputType::KeyDown;
        outEvent.button = EditorMouseButton::None;
        outEvent.key = key;
        return true;
    }

    case SDL_EVENT_TEXT_INPUT: {
        outEvent.type = EditorInputType::TextInput;
        outEvent.button = EditorMouseButton::None;
        const char* source = sdlEvent.text.text;
        if (source == nullptr || source[0] == '\0') {
            return false;
        }
        std::size_t i = 0;
        for (; i < sizeof(outEvent.text) - 1 && source[i] != '\0'; ++i) {
            outEvent.text[i] = source[i];
        }
        outEvent.text[i] = '\0';
        return true;
    }

    default:
        return false;
    }
}
