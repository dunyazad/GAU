#pragma once

// v2 link: normalized so fromPin is always the output side. Optional
// reroute waypoints route the curve.

#include "Ids.h"

#include <vector>

namespace gau {

struct LinkPoint
{
    float x = 0.0f;
    float y = 0.0f;
};

struct Link
{
    LinkId id = INVALID_ID;
    PinId fromPin = INVALID_ID;
    PinId toPin = INVALID_ID;
    std::vector<LinkPoint> points;
};

} // namespace gau
