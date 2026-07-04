#pragma once

#include "model/GraphTypes.h"

#include <nanovg.h>

#include <cmath>
#include <string>

// Deterministic color from a type name (FNV-1a hash -> hue), used to give
// each user-defined type a stable, distinct pin color.
inline NVGcolor UserTypeColor(const std::string& typeName)
{
    unsigned int hash = 2166136261u;
    for (char c : typeName) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    const float hue = static_cast<float>(hash % 360u) / 60.0f;
    const float saturation = 0.6f;
    const float value = 0.95f;
    const float chroma = value * saturation;
    const float xComp = chroma * (1.0f - std::fabs(hue - 2.0f * std::floor(hue / 2.0f) - 1.0f));
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    if (hue < 1.0f) {
        r = chroma;
        g = xComp;
    } else if (hue < 2.0f) {
        r = xComp;
        g = chroma;
    } else if (hue < 3.0f) {
        g = chroma;
        b = xComp;
    } else if (hue < 4.0f) {
        g = xComp;
        b = chroma;
    } else if (hue < 5.0f) {
        r = xComp;
        b = chroma;
    } else {
        r = chroma;
        b = xComp;
    }
    const float match = value - chroma;
    return nvgRGBf(r + match, g + match, b + match);
}

// Pin type colors from the design spec, shared by the node and link
// renderers.
inline NVGcolor PinColorForType(PinType type)
{
    switch (type) {
    case PinType::Exec:
        return nvgRGB(255, 255, 255);
    case PinType::Bool:
        return nvgRGB(140, 0, 0);
    case PinType::Int:
        return nvgRGB(30, 200, 160);
    case PinType::Float:
        return nvgRGB(160, 250, 60);
    case PinType::String:
        return nvgRGB(250, 0, 220);
    case PinType::Object:
        return nvgRGB(0, 160, 240);
    case PinType::UserType:
        // Fallback when no type name is available; the overload below
        // gives each user type a distinct hashed color.
        return nvgRGB(130, 60, 160);
    }
    return nvgRGB(255, 255, 255);
}

// Color for a pin/link, using the user type name for a stable per-type
// color when the type is user-defined.
inline NVGcolor PinColorForType(PinType type, const std::string& typeName)
{
    if (type == PinType::UserType && !typeName.empty()) {
        return UserTypeColor(typeName);
    }
    return PinColorForType(type);
}
