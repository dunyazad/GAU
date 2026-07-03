#pragma once

// Normalized input events, produced by platform/PlatformInput from SDL
// events and consumed by the interaction layer (InteractionFSM from M3).
// Pure data: this header must stay free of SDL and rendering includes.

enum class EditorInputType
{
    MouseMove,
    MouseDown,
    MouseUp,
    MouseWheel,
    KeyDown,
    TextInput,
};

enum class EditorMouseButton
{
    None,
    Left,
    Right,
    Middle,
};

enum class EditorKey
{
    None,
    Backspace,
    Delete,
    Escape,
    Enter,
    Tab,
    Undo,
    Redo,
    Copy,
    Paste,
};

struct EditorInputEvent
{
    EditorInputType type = EditorInputType::MouseMove;
    EditorMouseButton button = EditorMouseButton::None;
    EditorKey key = EditorKey::None;

    // Mouse position in logical window coordinates (screen space).
    float x = 0.0f;
    float y = 0.0f;

    // Modifiers held during a mouse event (alt = break links,
    // ctrl = insert reroute waypoint).
    bool alt = false;
    bool ctrl = false;

    // Vertical wheel movement in notches; positive means zoom in.
    float wheelDelta = 0.0f;

    // UTF-8 text for TextInput events (null-terminated).
    char text[8] = {};
};
