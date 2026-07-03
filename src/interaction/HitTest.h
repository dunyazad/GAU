#pragma once

#include "model/GraphTypes.h"

#include <vector>

class NodeLayoutCache;
class NodeGraph;
struct CommentNode;

// Pin hit radius, canvas units at zoom 1 (desktop; touch uses 24 in M6).
constexpr float PIN_HIT_RADIUS = 10.0f;

// Returns the topmost node whose body contains the canvas-space point,
// or INVALID_ID. Takes the render layer's per-frame layout cache as
// input (the one place interaction reads render output, per the
// architecture rules).
NodeId HitTestNode(const NodeLayoutCache& layoutCache, float canvasX, float canvasY);

// Returns the pin whose center is within PIN_HIT_RADIUS of the point,
// or INVALID_ID. Pins take priority over node bodies.
PinId HitTestPin(const NodeLayoutCache& layoutCache, float canvasX, float canvasY);

// Link curve hit distance, canvas units at zoom 1.
constexpr float LINK_HIT_TOLERANCE = 8.0f;
// Reroute waypoint hit radius.
constexpr float LINK_POINT_HIT_RADIUS = 9.0f;

// Returns the link whose curve (including waypoint segments) passes
// within LINK_HIT_TOLERANCE of the point, or INVALID_ID. When
// outSegmentIndex is non-null it receives the segment index, which is
// also the waypoint insertion index for ctrl-click rerouting.
// Priority: pin > waypoint > node > link > canvas.
LinkId HitTestLink(const NodeGraph& graph, const NodeLayoutCache& layoutCache,
                   float canvasX, float canvasY, int* outSegmentIndex = nullptr);

struct LinkPointHit
{
    LinkId linkId = INVALID_ID;
    int pointIndex = -1;
};

// Returns the reroute waypoint under the point, if any.
LinkPointHit HitTestLinkPoint(const NodeGraph& graph, float canvasX, float canvasY);

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
