// Node alignment, distribution and auto-layout math.

#include "Align.h"

#include <algorithm>

namespace gau {

namespace {

// Horizontal gap between two layout columns.
constexpr float LAYOUT_COLUMN_GAP = 80.0f;
// Vertical gap between two stacked nodes in the same layer.
constexpr float LAYOUT_ROW_GAP = 40.0f;

// Assigns each node a layer index via a longest-path pass (Kahn order) so
// that every edge points from a lower to a higher layer. Cyclic leftovers
// keep layer 0 (exec cycles are forbidden, data links do not cycle).
std::vector<int> ComputeLayers(const std::vector<std::vector<int>>& succ,
                               std::vector<int> indegree)
{
    const int n = static_cast<int>(succ.size());
    std::vector<int> layer(static_cast<std::size_t>(n), 0);
    std::vector<int> queue;
    for (int i = 0; i < n; ++i) {
        if (indegree[static_cast<std::size_t>(i)] == 0) {
            queue.push_back(i);
        }
    }
    for (std::size_t head = 0; head < queue.size(); ++head) {
        const int u = queue[head];
        for (int v : succ[static_cast<std::size_t>(u)]) {
            layer[static_cast<std::size_t>(v)] =
                std::max(layer[static_cast<std::size_t>(v)],
                         layer[static_cast<std::size_t>(u)] + 1);
            if (--indegree[static_cast<std::size_t>(v)] == 0) {
                queue.push_back(v);
            }
        }
    }
    return layer;
}

// Average predecessor slot; nodes without predecessors keep their order.
float Barycenter(int node, const std::vector<std::vector<int>>& pred,
                 const std::vector<int>& slot)
{
    const std::vector<int>& preds = pred[static_cast<std::size_t>(node)];
    if (preds.empty()) {
        return static_cast<float>(slot[static_cast<std::size_t>(node)]);
    }
    float sum = 0.0f;
    for (int p : preds) {
        sum += static_cast<float>(slot[static_cast<std::size_t>(p)]);
    }
    return sum / static_cast<float>(preds.size());
}

// Orders the nodes of each layer by the average slot of their predecessors
// (barycenter heuristic) to reduce link crossings. Starts from the current
// vertical order and runs a few downward sweeps.
void OrderWithinLayers(std::vector<std::vector<int>>& layers,
                       const std::vector<std::vector<int>>& pred,
                       const std::vector<NodeBox>& boxes)
{
    for (std::vector<int>& layerNodes : layers) {
        std::sort(layerNodes.begin(), layerNodes.end(), [&](int a, int b) {
            return boxes[static_cast<std::size_t>(a)].y < boxes[static_cast<std::size_t>(b)].y;
        });
    }

    const int n = static_cast<int>(boxes.size());
    for (int sweep = 0; sweep < 3; ++sweep) {
        std::vector<int> slot(static_cast<std::size_t>(n), 0);
        for (const std::vector<int>& layerNodes : layers) {
            for (int k = 0; k < static_cast<int>(layerNodes.size()); ++k) {
                slot[static_cast<std::size_t>(layerNodes[static_cast<std::size_t>(k)])] = k;
            }
        }
        for (std::vector<int>& layerNodes : layers) {
            std::stable_sort(layerNodes.begin(), layerNodes.end(), [&](int a, int b) {
                return Barycenter(a, pred, slot) < Barycenter(b, pred, slot);
            });
        }
    }
}

} // namespace

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

std::vector<NodePos> ComputeAutoLayout(const std::vector<NodeBox>& boxes,
                                       const std::vector<LayoutEdge>& edges)
{
    std::vector<NodePos> out;
    out.reserve(boxes.size());
    for (const NodeBox& b : boxes) {
        out.push_back(NodePos{b.id, b.x, b.y});
    }
    const int n = static_cast<int>(boxes.size());
    if (n < 2) {
        return out;
    }

    std::vector<std::vector<int>> succ(static_cast<std::size_t>(n));
    std::vector<std::vector<int>> pred(static_cast<std::size_t>(n));
    std::vector<int> indegree(static_cast<std::size_t>(n), 0);
    for (const LayoutEdge& edge : edges) {
        if (edge.from < 0 || edge.from >= n || edge.to < 0 || edge.to >= n
            || edge.from == edge.to) {
            continue;
        }
        succ[static_cast<std::size_t>(edge.from)].push_back(edge.to);
        pred[static_cast<std::size_t>(edge.to)].push_back(edge.from);
        ++indegree[static_cast<std::size_t>(edge.to)];
    }

    const std::vector<int> layer = ComputeLayers(succ, indegree);
    int maxLayer = 0;
    for (int l : layer) {
        maxLayer = std::max(maxLayer, l);
    }
    std::vector<std::vector<int>> layers(static_cast<std::size_t>(maxLayer + 1));
    for (int i = 0; i < n; ++i) {
        layers[static_cast<std::size_t>(layer[static_cast<std::size_t>(i)])].push_back(i);
    }
    OrderWithinLayers(layers, pred, boxes);

    float originX = boxes[0].x;
    float originY = boxes[0].y;
    for (const NodeBox& b : boxes) {
        originX = std::min(originX, b.x);
        originY = std::min(originY, b.y);
    }

    std::vector<float> layerWidth(static_cast<std::size_t>(maxLayer + 1), 0.0f);
    for (int i = 0; i < n; ++i) {
        const std::size_t l = static_cast<std::size_t>(layer[static_cast<std::size_t>(i)]);
        layerWidth[l] = std::max(layerWidth[l], boxes[static_cast<std::size_t>(i)].w);
    }

    float columnX = originX;
    for (int l = 0; l <= maxLayer; ++l) {
        float rowY = originY;
        for (int nodeIndex : layers[static_cast<std::size_t>(l)]) {
            const NodeBox& b = boxes[static_cast<std::size_t>(nodeIndex)];
            out[static_cast<std::size_t>(nodeIndex)].x = columnX;
            out[static_cast<std::size_t>(nodeIndex)].y = rowY;
            rowY += b.h + LAYOUT_ROW_GAP;
        }
        columnX += layerWidth[static_cast<std::size_t>(l)] + LAYOUT_COLUMN_GAP;
    }
    return out;
}

} // namespace gau
