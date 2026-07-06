// v2 hit testing.

#include "HitTest2.h"

#include <algorithm>

namespace gau {

NodeId HitTestNode(const render::GraphLayout& layout, float canvasX, float canvasY)
{
    const std::vector<render::NodeLayout>& nodes = layout.Nodes();
    // Reverse order so the topmost (last drawn) node wins.
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
        if (canvasX >= it->x && canvasX <= it->x + it->w && canvasY >= it->y
            && canvasY <= it->y + it->h) {
            return it->id;
        }
    }
    return INVALID_ID;
}

PinId HitTestPin(const render::GraphLayout& layout, float canvasX, float canvasY, float radius)
{
    const float r2 = radius * radius;
    for (const render::NodeLayout& node : layout.Nodes()) {
        for (const render::PinLayout& pin : node.pins) {
            const float dx = pin.x - canvasX;
            const float dy = pin.y - canvasY;
            if (dx * dx + dy * dy <= r2) {
                return pin.id;
            }
        }
    }
    return INVALID_ID;
}

std::vector<NodeId> HitTestNodesInRect(const render::GraphLayout& layout, float x0, float y0,
                                       float x1, float y1)
{
    const float minX = std::min(x0, x1);
    const float maxX = std::max(x0, x1);
    const float minY = std::min(y0, y1);
    const float maxY = std::max(y0, y1);
    std::vector<NodeId> hit;
    for (const render::NodeLayout& node : layout.Nodes()) {
        const bool intersects = node.x <= maxX && node.x + node.w >= minX && node.y <= maxY
                             && node.y + node.h >= minY;
        if (intersects) {
            hit.push_back(node.id);
        }
    }
    return hit;
}

namespace {

// Signed orientation of the triangle (a, b, c).
float Orientation(float ax, float ay, float bx, float by, float cx, float cy)
{
    return (bx - ax) * (cy - ay) - (by - ay) * (cx - ax);
}

bool SegmentsIntersect(float ax, float ay, float bx, float by, float cx, float cy, float dx,
                       float dy)
{
    const float o1 = Orientation(ax, ay, bx, by, cx, cy);
    const float o2 = Orientation(ax, ay, bx, by, dx, dy);
    const float o3 = Orientation(cx, cy, dx, dy, ax, ay);
    const float o4 = Orientation(cx, cy, dx, dy, bx, by);
    return ((o1 > 0.0f) != (o2 > 0.0f)) && ((o3 > 0.0f) != (o4 > 0.0f));
}

// Invokes fn(prevX, prevY, x, y) for each sampled segment of the link's
// cubic (the same geometry the renderer draws).
template <typename Fn>
void ForEachLinkSegment(const render::PinLayout& from, const render::PinLayout& to, Fn fn)
{
    const float dx = to.x - from.x;
    const float tangent = (dx < 200.0f && dx > -200.0f) ? 100.0f : dx * 0.5f;
    const float cx0 = from.x + tangent;
    const float cx1 = to.x - tangent;
    const int STEPS = 24;
    float prevX = from.x;
    float prevY = from.y;
    for (int i = 1; i <= STEPS; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(STEPS);
        const float u = 1.0f - t;
        const float px = u * u * u * from.x + 3.0f * u * u * t * cx0 + 3.0f * u * t * t * cx1
                       + t * t * t * to.x;
        const float py = u * u * u * from.y + 3.0f * u * u * t * from.y
                       + 3.0f * u * t * t * to.y + t * t * t * to.y;
        fn(prevX, prevY, px, py);
        prevX = px;
        prevY = py;
    }
}

} // namespace

LinkId HitTestLink(const Graph& graph, const render::GraphLayout& layout, float canvasX,
                   float canvasY, float radius)
{
    const float radiusSq = radius * radius;
    LinkId best = INVALID_ID;
    float bestDistSq = radiusSq;
    for (const Link& link : graph.Links()) {
        const render::PinLayout* from = layout.FindPin(link.fromPin);
        const render::PinLayout* to = layout.FindPin(link.toPin);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        // Closest point-to-segment distance over the sampled curve.
        ForEachLinkSegment(*from, *to, [&](float x0, float y0, float x1, float y1) {
            const float segX = x1 - x0;
            const float segY = y1 - y0;
            const float lenSq = segX * segX + segY * segY;
            float s = 0.0f;
            if (lenSq > 0.0f) {
                s = ((canvasX - x0) * segX + (canvasY - y0) * segY) / lenSq;
                s = std::max(0.0f, std::min(1.0f, s));
            }
            const float nearX = x0 + segX * s;
            const float nearY = y0 + segY * s;
            const float distSq = (canvasX - nearX) * (canvasX - nearX)
                               + (canvasY - nearY) * (canvasY - nearY);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                best = link.id;
            }
        });
    }
    return best;
}

std::vector<LinkId> HitTestLinksCrossing(const Graph& graph, const render::GraphLayout& layout,
                                         const std::vector<CutPoint>& polyline)
{
    std::vector<LinkId> crossed;
    if (polyline.size() < 2) {
        return crossed;
    }
    for (const Link& link : graph.Links()) {
        const render::PinLayout* from = layout.FindPin(link.fromPin);
        const render::PinLayout* to = layout.FindPin(link.toPin);
        if (from == nullptr || to == nullptr) {
            continue;
        }
        bool hit = false;
        ForEachLinkSegment(*from, *to, [&](float x0, float y0, float x1, float y1) {
            if (hit) {
                return;
            }
            for (std::size_t i = 1; i < polyline.size(); ++i) {
                if (SegmentsIntersect(x0, y0, x1, y1, polyline[i - 1].x, polyline[i - 1].y,
                                      polyline[i].x, polyline[i].y)) {
                    hit = true;
                    return;
                }
            }
        });
        if (hit) {
            crossed.push_back(link.id);
        }
    }
    return crossed;
}

} // namespace gau
