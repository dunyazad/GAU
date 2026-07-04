// Node alignment and distribution math.

#include "Align.h"

#include <algorithm>

namespace gau {

std::vector<NodePos> ComputeAlign(const std::vector<NodeBox>& boxes, AlignMode mode)
{
    std::vector<NodePos> out;
    out.reserve(boxes.size());
    for (const NodeBox& b : boxes) {
        out.push_back(NodePos{b.id, b.x, b.y});
    }
    if (boxes.size() < 2) {
        return out;
    }

    float minX = boxes[0].x;
    float maxRight = boxes[0].x + boxes[0].w;
    float minY = boxes[0].y;
    float maxBottom = boxes[0].y + boxes[0].h;
    float sumCenterX = 0.0f;
    float sumCenterY = 0.0f;
    for (const NodeBox& b : boxes) {
        minX = std::min(minX, b.x);
        maxRight = std::max(maxRight, b.x + b.w);
        minY = std::min(minY, b.y);
        maxBottom = std::max(maxBottom, b.y + b.h);
        sumCenterX += b.x + b.w * 0.5f;
        sumCenterY += b.y + b.h * 0.5f;
    }
    const float avgCenterX = sumCenterX / static_cast<float>(boxes.size());
    const float avgCenterY = sumCenterY / static_cast<float>(boxes.size());

    for (std::size_t k = 0; k < boxes.size(); ++k) {
        const NodeBox& b = boxes[k];
        switch (mode) {
        case AlignMode::Left:
            out[k].x = minX;
            break;
        case AlignMode::Right:
            out[k].x = maxRight - b.w;
            break;
        case AlignMode::Top:
            out[k].y = minY;
            break;
        case AlignMode::Bottom:
            out[k].y = maxBottom - b.h;
            break;
        case AlignMode::CenterHorizontal:
            out[k].x = avgCenterX - b.w * 0.5f;
            break;
        case AlignMode::CenterVertical:
            out[k].y = avgCenterY - b.h * 0.5f;
            break;
        }
    }
    return out;
}

std::vector<NodePos> ComputeDistribute(const std::vector<NodeBox>& boxes, bool horizontal)
{
    std::vector<NodePos> out;
    out.reserve(boxes.size());
    for (const NodeBox& b : boxes) {
        out.push_back(NodePos{b.id, b.x, b.y});
    }
    if (boxes.size() < 3) {
        return out;
    }

    // Order boxes by center along the axis; keep the ends fixed.
    std::vector<std::size_t> order(boxes.size());
    for (std::size_t k = 0; k < boxes.size(); ++k) {
        order[k] = k;
    }
    const auto center = [&boxes, horizontal](std::size_t k) {
        const NodeBox& b = boxes[k];
        return horizontal ? (b.x + b.w * 0.5f) : (b.y + b.h * 0.5f);
    };
    std::sort(order.begin(), order.end(),
              [&center](std::size_t a, std::size_t b) { return center(a) < center(b); });

    const float first = center(order.front());
    const float last = center(order.back());
    const float step = (last - first) / static_cast<float>(order.size() - 1);
    for (std::size_t i = 1; i + 1 < order.size(); ++i) {
        const std::size_t k = order[i];
        const float target = first + step * static_cast<float>(i);
        if (horizontal) {
            out[k].x = target - boxes[k].w * 0.5f;
        } else {
            out[k].y = target - boxes[k].h * 0.5f;
        }
    }
    return out;
}

} // namespace gau
