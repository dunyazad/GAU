#include "HitTest.h"

#include "model/NodeGraph.h"
#include "render/NodeLayoutCache.h"

std::vector<NodeId> HitTestNodesInRect(const NodeLayoutCache& layoutCache,
                                       float x0, float y0, float x1, float y1)
{
    const float left = (x0 < x1) ? x0 : x1;
    const float right = (x0 < x1) ? x1 : x0;
    const float top = (y0 < y1) ? y0 : y1;
    const float bottom = (y0 < y1) ? y1 : y0;

    std::vector<NodeId> result;
    for (const NodeLayout& layout : layoutCache.GetAll()) {
        const bool intersects = layout.x <= right && layout.x + layout.width >= left
                             && layout.y <= bottom && layout.y + layout.height >= top;
        if (intersects) {
            result.push_back(layout.nodeId);
        }
    }
    return result;
}

NodeId HitTestNode(const NodeLayoutCache& layoutCache, float canvasX, float canvasY)
{
    const std::vector<NodeLayout>& layouts = layoutCache.GetAll();
    // Iterate backwards: later layouts draw on top.
    for (std::size_t i = layouts.size(); i > 0; --i) {
        const NodeLayout& layout = layouts[i - 1];
        if (canvasX >= layout.x && canvasX <= layout.x + layout.width
            && canvasY >= layout.y && canvasY <= layout.y + layout.height) {
            return layout.nodeId;
        }
    }
    return INVALID_ID;
}

PinId HitTestPin(const NodeLayoutCache& layoutCache, float canvasX, float canvasY)
{
    const float radiusSquared = PIN_HIT_RADIUS * PIN_HIT_RADIUS;
    const std::vector<NodeLayout>& layouts = layoutCache.GetAll();
    for (std::size_t i = layouts.size(); i > 0; --i) {
        for (const PinLayout& pin : layouts[i - 1].pins) {
            const float dx = canvasX - pin.x;
            const float dy = canvasY - pin.y;
            if (dx * dx + dy * dy <= radiusSquared) {
                return pin.pinId;
            }
        }
    }
    return INVALID_ID;
}

static float DistanceSquaredToSegment(float px, float py, float ax, float ay, float bx, float by)
{
    const float abx = bx - ax;
    const float aby = by - ay;
    const float lengthSquared = abx * abx + aby * aby;
    float t = 0.0f;
    if (lengthSquared > 0.0f) {
        t = ((px - ax) * abx + (py - ay) * aby) / lengthSquared;
        if (t < 0.0f) {
            t = 0.0f;
        }
        if (t > 1.0f) {
            t = 1.0f;
        }
    }
    const float dx = px - (ax + abx * t);
    const float dy = py - (ay + aby * t);
    return dx * dx + dy * dy;
}

// Anchor list a link's curve passes through: from pin, waypoints, to pin.
static std::vector<LinkPoint> LinkAnchors(const Link& link, const PinLayout& fromPin,
                                          const PinLayout& toPin)
{
    std::vector<LinkPoint> anchors;
    anchors.push_back(LinkPoint{fromPin.x, fromPin.y});
    for (const LinkPoint& point : link.points) {
        anchors.push_back(point);
    }
    anchors.push_back(LinkPoint{toPin.x, toPin.y});
    return anchors;
}

// True when the point is within tolerance of the bezier segment between
// two anchors (the same curve the renderer draws).
static bool HitsBezierSegment(float canvasX, float canvasY, const LinkPoint& a,
                              const LinkPoint& b, float toleranceSquared)
{
    const int SAMPLE_COUNT = 24;
    const float tangent = LinkTangent(b.x - a.x);
    const float c1x = a.x + tangent;
    const float c2x = b.x - tangent;

    float previousX = a.x;
    float previousY = a.y;
    for (int sample = 1; sample <= SAMPLE_COUNT; ++sample) {
        const float t = static_cast<float>(sample) / static_cast<float>(SAMPLE_COUNT);
        const float u = 1.0f - t;
        const float sampleX = u * u * u * a.x + 3.0f * u * u * t * c1x
                            + 3.0f * u * t * t * c2x + t * t * t * b.x;
        const float sampleY = u * u * u * a.y + 3.0f * u * u * t * a.y
                            + 3.0f * u * t * t * b.y + t * t * t * b.y;
        if (DistanceSquaredToSegment(canvasX, canvasY, previousX, previousY,
                                     sampleX, sampleY) <= toleranceSquared) {
            return true;
        }
        previousX = sampleX;
        previousY = sampleY;
    }
    return false;
}

LinkId HitTestLink(const NodeGraph& graph, const NodeLayoutCache& layoutCache,
                   float canvasX, float canvasY, int* outSegmentIndex)
{
    const float toleranceSquared = LINK_HIT_TOLERANCE * LINK_HIT_TOLERANCE;

    const std::vector<Link>& links = graph.GetLinks();
    for (std::size_t i = links.size(); i > 0; --i) {
        const Link& link = links[i - 1];
        const PinLayout* fromPin = layoutCache.FindPin(link.fromPinId);
        const PinLayout* toPin = layoutCache.FindPin(link.toPinId);
        if (fromPin == nullptr || toPin == nullptr) {
            continue;
        }

        const std::vector<LinkPoint> anchors = LinkAnchors(link, *fromPin, *toPin);
        for (std::size_t segment = 0; segment + 1 < anchors.size(); ++segment) {
            if (HitsBezierSegment(canvasX, canvasY, anchors[segment], anchors[segment + 1],
                                  toleranceSquared)) {
                if (outSegmentIndex != nullptr) {
                    *outSegmentIndex = static_cast<int>(segment);
                }
                return link.id;
            }
        }
    }
    return INVALID_ID;
}

LinkPointHit HitTestLinkPoint(const NodeGraph& graph, float canvasX, float canvasY)
{
    LinkPointHit hit;
    const float radiusSquared = LINK_POINT_HIT_RADIUS * LINK_POINT_HIT_RADIUS;

    const std::vector<Link>& links = graph.GetLinks();
    for (std::size_t i = links.size(); i > 0; --i) {
        const Link& link = links[i - 1];
        for (int pointIndex = 0; pointIndex < static_cast<int>(link.points.size());
             ++pointIndex) {
            const LinkPoint& point = link.points[static_cast<std::size_t>(pointIndex)];
            const float dx = canvasX - point.x;
            const float dy = canvasY - point.y;
            if (dx * dx + dy * dy <= radiusSquared) {
                hit.linkId = link.id;
                hit.pointIndex = pointIndex;
                return hit;
            }
        }
    }
    return hit;
}

CommentId HitTestCommentTitle(const NodeGraph& graph, float canvasX, float canvasY)
{
    const std::vector<CommentNode>& comments = graph.GetComments();
    for (std::size_t i = comments.size(); i > 0; --i) {
        const CommentNode& comment = comments[i - 1];
        if (canvasX >= comment.x && canvasX <= comment.x + comment.width
            && canvasY >= comment.y && canvasY <= comment.y + COMMENT_TITLE_HEIGHT) {
            return comment.id;
        }
    }
    return INVALID_ID;
}

CommentId HitTestCommentResizeHandle(const NodeGraph& graph, float canvasX, float canvasY)
{
    const std::vector<CommentNode>& comments = graph.GetComments();
    for (std::size_t i = comments.size(); i > 0; --i) {
        const CommentNode& comment = comments[i - 1];
        const float right = comment.x + comment.width;
        const float bottom = comment.y + comment.height;
        if (canvasX >= right - COMMENT_RESIZE_HANDLE && canvasX <= right
            && canvasY >= bottom - COMMENT_RESIZE_HANDLE && canvasY <= bottom) {
            return comment.id;
        }
    }
    return INVALID_ID;
}

std::vector<NodeId> NodesContainedInComment(const NodeLayoutCache& layoutCache,
                                            const CommentNode& comment)
{
    std::vector<NodeId> result;
    for (const NodeLayout& layout : layoutCache.GetAll()) {
        const bool contained = layout.x >= comment.x
                            && layout.y >= comment.y
                            && layout.x + layout.width <= comment.x + comment.width
                            && layout.y + layout.height <= comment.y + comment.height;
        if (contained) {
            result.push_back(layout.nodeId);
        }
    }
    return result;
}
