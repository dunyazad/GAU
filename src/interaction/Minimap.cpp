// Minimap fit transform and comment grouping query.

#include "Minimap.h"

#include <algorithm>

namespace gau {

MinimapFit ComputeMinimap(const Bounds& content, const ViewRect& panel, const Bounds& visible)
{
    MinimapFit fit;
    const float worldW = std::max(content.maxX - content.minX, 1.0f);
    const float worldH = std::max(content.maxY - content.minY, 1.0f);
    fit.scale = std::min(panel.w / worldW, panel.h / worldH);
    // Center the scaled content within the panel.
    fit.offsetX = panel.x + (panel.w - worldW * fit.scale) * 0.5f - content.minX * fit.scale;
    fit.offsetY = panel.y + (panel.h - worldH * fit.scale) * 0.5f - content.minY * fit.scale;

    fit.viewport.x = fit.offsetX + visible.minX * fit.scale;
    fit.viewport.y = fit.offsetY + visible.minY * fit.scale;
    fit.viewport.w = (visible.maxX - visible.minX) * fit.scale;
    fit.viewport.h = (visible.maxY - visible.minY) * fit.scale;
    return fit;
}

std::vector<NodeId> NodesInRect(const std::vector<NodeBox>& boxes, const ViewRect& rect)
{
    std::vector<NodeId> inside;
    const float rx0 = rect.x;
    const float ry0 = rect.y;
    const float rx1 = rect.x + rect.w;
    const float ry1 = rect.y + rect.h;
    for (const NodeBox& b : boxes) {
        if (b.x >= rx0 && b.y >= ry0 && b.x + b.w <= rx1 && b.y + b.h <= ry1) {
            inside.push_back(b.id);
        }
    }
    return inside;
}

} // namespace gau
