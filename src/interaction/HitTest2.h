#pragma once

// v2 hit testing over a render::GraphLayout (canvas-space). Priority
// mirrors v1: pin > node. (File is HitTest2 to coexist with the v1
// HitTest.h; symbols live in namespace gau.)

#include "model/Graph.h"
#include "model/Ids.h"
#include "render/GraphLayout.h"

#include <vector>

namespace gau {

// Topmost node whose body contains the canvas point, or INVALID_ID.
NodeId HitTestNode(const render::GraphLayout& layout, float canvasX, float canvasY);

// Pin whose center is within radius of the point, or INVALID_ID.
PinId HitTestPin(const render::GraphLayout& layout, float canvasX, float canvasY, float radius);

// Nodes whose body intersects the canvas rectangle (corners in any order).
std::vector<NodeId> HitTestNodesInRect(const render::GraphLayout& layout, float x0, float y0,
                                       float x1, float y1);

// Link whose curve passes within radius of the point, or INVALID_ID.
// The curve mirrors the renderer's cubic (horizontal tangents), sampled
// as short segments.
LinkId HitTestLink(const Graph& graph, const render::GraphLayout& layout, float canvasX,
                   float canvasY, float radius);

} // namespace gau
