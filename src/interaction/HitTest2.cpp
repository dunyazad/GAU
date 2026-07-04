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

} // namespace gau
