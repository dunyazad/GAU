#pragma once

#include "model/GraphTypes.h"

#include <vector>

class NodeLayoutCache;
class NodeGraph;
struct CommentNode;

// Returns the topmost node whose body contains the canvas-space point,
// or INVALID_ID. Takes the render layer's per-frame layout cache as
// input (the one place interaction reads render output, per the
// architecture rules). Pin/link hit testing arrives with M4.
NodeId HitTestNode(const NodeLayoutCache& layoutCache, float canvasX, float canvasY);

// Returns all nodes whose body intersects the canvas-space rectangle
// (corners in any order). Used by rubber-band selection.
std::vector<NodeId> HitTestNodesInRect(const NodeLayoutCache& layoutCache,
                                       float x0, float y0, float x1, float y1);

// Comment boxes are hit only on their title bar (drag/rename) and the
// bottom-right resize handle; the translucent body passes clicks
// through. Both read comment rects straight from the model.
CommentId HitTestCommentTitle(const NodeGraph& graph, float canvasX, float canvasY);
CommentId HitTestCommentResizeHandle(const NodeGraph& graph, float canvasX, float canvasY);

// Nodes whose layout rect is fully inside the comment rect; these move
// together with the comment.
std::vector<NodeId> NodesContainedInComment(const NodeLayoutCache& layoutCache,
                                            const CommentNode& comment);
