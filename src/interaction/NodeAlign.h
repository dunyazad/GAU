#pragma once

// Blueprint-style alignment of the selected node set. Reads the render
// layer's per-frame layout cache (node bounds and pin positions) and
// produces MoveNodesCommand input; the caller executes it through the
// undo stack. This is the second place interaction reads render output,
// mirroring HitTest.

#include "model/GraphTypes.h"
#include "model/GraphCommands.h"

#include <vector>

class NodeGraph;
class NodeLayoutCache;

// Order is significant: the node action menu appends items in this order
// and dispatches by (itemIndex - firstAlignIndex).
enum class AlignOp
{
    Left,
    CenterX,
    Right,
    Top,
    CenterY,
    Bottom,
    DistributeHorizontal,
    DistributeVertical,
    Straighten,
};

// Computes the position changes needed to apply op to the selected nodes.
// Returns an empty vector when there is nothing to do (too few nodes for
// the op, missing layout, or no node actually moves). Only nodes that
// change position are included.
std::vector<NodeMove> ComputeAlignMoves(AlignOp op,
                                        const std::vector<NodeId>& selectedNodes,
                                        const NodeGraph& graph,
                                        const NodeLayoutCache& layoutCache);

// Arranges nodesToLayout into a left-to-right layered layout: layers
// (columns) come from the longest path over the directed links internal
// to the set, and nodes within a layer are ordered by the average row of
// their predecessors to reduce link crossings. The layout is anchored at
// the top-left of the set's current bounding box. Returns MoveNodesCommand
// input; empty when fewer than two nodes have layout.
std::vector<NodeMove> ComputeAutoLayoutMoves(const std::vector<NodeId>& nodesToLayout,
                                             const NodeGraph& graph,
                                             const NodeLayoutCache& layoutCache);
