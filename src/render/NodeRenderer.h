#pragma once

#include "NodeLayoutCache.h"
#include "model/GraphTypes.h"

struct NVGcontext;
struct Node;
class NodeGraph;

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
// Uses vg only for text measurement. Stateless. graph and previewValues
// size the body to any live linked-property text.
NodeLayout ComputeNodeLayout(NVGcontext* vg, const Node& node,
                             const NodeGraph& graph, const PinValueCache& previewValues);

// Draws one node using a layout produced by ComputeNodeLayout.
// Assumes the canvas transform is already applied to vg. Stateless.
// graph and previewValues let property rows whose same-named input pin
// is linked show the evaluated source value instead of the static
// property.
void DrawNode(NVGcontext* vg, const Node& node, const NodeLayout& layout, bool selected,
              const NodeGraph& graph, const PinValueCache& previewValues);
