#pragma once

// Node search and focus (SRS FR-UX-2). Case-insensitive substring match on
// node class names, plus a bounding-box helper the app uses to center the
// view on the matches. Pure logic, unit-testable.

#include "Align.h" // NodeBox

#include "model/Graph.h"
#include "model/Ids.h"

#include <string>
#include <vector>

namespace gau {

// Returns the ids of nodes whose class name contains `query`
// (case-insensitive), in graph order. An empty query matches nothing.
std::vector<NodeId> SearchNodes(const Graph& graph, const std::string& query);

struct Bounds
{
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;
};

// Axis-aligned bounds enclosing the given boxes. False when empty.
bool ComputeBounds(const std::vector<NodeBox>& boxes, Bounds& out);

} // namespace gau
