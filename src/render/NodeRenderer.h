#pragma once

#include "NodeLayoutCache.h"

struct NVGcontext;
struct Node;

// Node visual constants from the design spec (canvas units at zoom 1).
constexpr float NODE_CORNER_RADIUS = 6.0f;
constexpr float NODE_MIN_WIDTH = 160.0f;
constexpr float NODE_HEADER_HEIGHT = 26.0f;
constexpr float NODE_PIN_ROW_HEIGHT = 24.0f;
constexpr float NODE_PIN_INSET = 12.0f;
constexpr float NODE_BODY_PADDING = 8.0f;
constexpr float DATA_PIN_RADIUS = 5.0f;
constexpr float NODE_PROPERTY_ROW_HEIGHT = 20.0f;
constexpr float NODE_PROPERTY_SECTION_GAP = 4.0f;

// Computes the canvas-space layout (size, pin centers) for a node.
// Uses vg only for text measurement. Stateless.
NodeLayout ComputeNodeLayout(NVGcontext* vg, const Node& node);

// Draws one node using a layout produced by ComputeNodeLayout.
// Assumes the canvas transform is already applied to vg. Stateless.
void DrawNode(NVGcontext* vg, const Node& node, const NodeLayout& layout, bool selected);
