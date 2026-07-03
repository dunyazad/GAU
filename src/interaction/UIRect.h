#pragma once

// Screen-space rectangle shared by UI hit testing and rendering.
struct UIRect
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
