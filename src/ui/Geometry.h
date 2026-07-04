#pragma once

// UI geometry, color and normalized events. Pure data; the UI toolkit
// stays free of NanoVG/SDL so it is unit-testable without a GL context.

#include <cstdint>

namespace gau::ui {

struct Size
{
    float w = 0.0f;
    float h = 0.0f;
};

struct Rect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(float px, float py) const
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

struct Color
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 255;
};

enum class EventType
{
    MouseMove,
    MouseDown,
    MouseUp,
    Wheel,
    Key,
    Text,
};

struct Event
{
    EventType type = EventType::MouseMove;
    float x = 0.0f;
    float y = 0.0f;
    int button = 0;   // 0 = left
    float wheel = 0.0f;
    int key = 0;
    char text[8] = {};

    bool IsPositional() const
    {
        return type == EventType::MouseMove || type == EventType::MouseDown
            || type == EventType::MouseUp || type == EventType::Wheel;
    }
};

} // namespace gau::ui
