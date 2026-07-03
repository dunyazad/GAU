#pragma once

#include "model/GraphTypes.h"

#include <nanovg.h>

#include <string>

// Presentation helpers for free-form category strings, shared by the
// node renderer, context menu and class editor dialog.

// Builtin categories use the design-spec palette; unknown (user-defined)
// categories get a stable hash-derived hue so each reads distinctly.
inline NVGcolor CategoryColor(const std::string& category)
{
    if (category == "Event") {
        return nvgRGB(150, 30, 30);
    }
    if (category == "Function") {
        return nvgRGB(40, 80, 160);
    }
    if (category == "FlowControl") {
        return nvgRGB(90, 90, 100);
    }
    if (category == "Pure") {
        return nvgRGB(60, 120, 60);
    }
    if (category == "Object") {
        return nvgRGB(0, 120, 180);
    }
    if (category == "CustomObject") {
        return nvgRGB(130, 60, 160);
    }

    // FNV-1a hash -> hue.
    unsigned int hash = 2166136261u;
    for (char c : category) {
        hash = (hash ^ static_cast<unsigned char>(c)) * 16777619u;
    }
    const float hue = static_cast<float>(hash % 360u) / 360.0f;
    return nvgHSL(hue, 0.5f, 0.35f);
}

// Spaced labels for the builtin compound names; other categories are
// shown verbatim.
inline std::string CategoryDisplayName(const std::string& category)
{
    if (category == "FlowControl") {
        return "Flow Control";
    }
    if (category == "CustomObject") {
        return "Custom Object";
    }
    return category;
}
