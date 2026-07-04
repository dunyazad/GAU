#pragma once

// Minimap fit transform (SRS FR-UX-1) and comment grouping query
// (SRS FR-UX-4). Pure view math: maps world content into a minimap panel
// preserving aspect, and reports which nodes fall inside a rectangle (used
// to move a comment box with its enclosed nodes).

#include "Align.h"      // NodeBox
#include "NodeSearch.h" // Bounds

#include "model/Ids.h"

#include <vector>

namespace gau {

struct ViewRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct MinimapFit
{
    float scale = 1.0f;   // world units -> panel pixels
    float offsetX = 0.0f; // panelX = offsetX + worldX * scale
    float offsetY = 0.0f;
    ViewRect viewport;    // the visible world region, mapped into the panel
};

// Fits `content` into `panel` (aspect-preserving, centered) and maps the
// currently `visible` world rectangle into panel space.
MinimapFit ComputeMinimap(const Bounds& content, const ViewRect& panel, const Bounds& visible);

// Node ids whose box is fully contained in `rect` (world space).
std::vector<NodeId> NodesInRect(const std::vector<NodeBox>& boxes, const ViewRect& rect);

} // namespace gau
