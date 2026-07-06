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
        const float dx = to->x - from->x;
        const float tangent = (dx < 200.0f && dx > -200.0f) ? 100.0f : dx * 0.5f;
        const float cx0 = from->x + tangent;
        const float cx1 = to->x - tangent;

        // Sample the cubic as short segments and take the closest
        // point-to-segment distance.
        const int STEPS = 24;
        float prevX = from->x;
        float prevY = from->y;
        for (int i = 1; i <= STEPS; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(STEPS);
            const float u = 1.0f - t;
            const float px = u * u * u * from->x + 3.0f * u * u * t * cx0
                           + 3.0f * u * t * t * cx1 + t * t * t * to->x;
            const float py = u * u * u * from->y + 3.0f * u * u * t * from->y
                           + 3.0f * u * t * t * to->y + t * t * t * to->y;
            const float segX = px - prevX;
            const float segY = py - prevY;
            const float lenSq = segX * segX + segY * segY;
            float s = 0.0f;
            if (lenSq > 0.0f) {
                s = ((canvasX - prevX) * segX + (canvasY - prevY) * segY) / lenSq;
                s = std::max(0.0f, std::min(1.0f, s));
            }
            const float nearX = prevX + segX * s;
            const float nearY = prevY + segY * s;
            const float distSq = (canvasX - nearX) * (canvasX - nearX)
                               + (canvasY - nearY) * (canvasY - nearY);
            if (distSq < bestDistSq) {
                bestDistSq = distSq;
                best = link.id;
            }
            prevX = px;
            prevY = py;
        }
    }
    return best;
}

} // namespace gau
