#pragma once

#include "model/GraphTypes.h"

#include <nanovg.h>

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
    }
    return nvgRGB(255, 255, 255);
}
