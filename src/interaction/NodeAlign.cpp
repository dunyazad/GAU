// Blueprint-style alignment: edge/center alignment, even distribution and
// connection straightening for the selected node set.

#include "NodeAlign.h"

#include "model/NodeGraph.h"
#include "render/NodeLayoutCache.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <unordered_map>
#include <unordered_set>

namespace {

// Layout snapshot of one selected node (canvas-space bounds).
struct SelectedBounds
{
    NodeId nodeId = INVALID_ID;
    float x = 0.0f;
    float y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

// Nodes below this position delta are treated as already aligned.
constexpr float MOVE_EPSILON = 0.5f;

std::vector<SelectedBounds> GatherBounds(const std::vector<NodeId>& selectedNodes,
                                         const NodeLayoutCache& layoutCache)
{
    std::vector<SelectedBounds> result;
    for (NodeId nodeId : selectedNodes) {
        const NodeLayout* layout = layoutCache.Find(nodeId);
        if (layout != nullptr) {
            result.push_back(
                SelectedBounds{nodeId, layout->x, layout->y, layout->width, layout->height});
        }
    }
    return result;
}

NodeMove MakeMove(const NodeGraph& graph, NodeId nodeId, float toX, float toY)
{
    NodeMove move;
    move.nodeId = nodeId;
    const Node* node = graph.FindNode(nodeId);
    move.fromX = node->x;
    move.fromY = node->y;
    move.toX = toX;
    move.toY = toY;
    return move;
}

void AppendIfMoved(std::vector<NodeMove>& moves, const NodeGraph& graph, NodeId nodeId,
                   float toX, float toY)
{
    const Node* node = graph.FindNode(nodeId);
    if (node == nullptr) {
        return;
    }
    if (std::fabs(node->x - toX) < MOVE_EPSILON && std::fabs(node->y - toY) < MOVE_EPSILON) {
        return;
    }
    moves.push_back(MakeMove(graph, nodeId, toX, toY));
}

// Left/CenterX/Right/Top/CenterY/Bottom: shift every node to a single
// reference coordinate on one axis, keeping the other axis fixed.
std::vector<NodeMove> AlignEdges(AlignOp op, const std::vector<SelectedBounds>& bounds,
                                 const NodeGraph& graph)
{
    float minLeft = bounds[0].x;
    float maxRight = bounds[0].x + bounds[0].width;
    float minTop = bounds[0].y;
    float maxBottom = bounds[0].y + bounds[0].height;
    for (const SelectedBounds& b : bounds) {
        minLeft = std::min(minLeft, b.x);
        maxRight = std::max(maxRight, b.x + b.width);
        minTop = std::min(minTop, b.y);
        maxBottom = std::max(maxBottom, b.y + b.height);
    }
    const float centerX = (minLeft + maxRight) * 0.5f;
    const float centerY = (minTop + maxBottom) * 0.5f;

    std::vector<NodeMove> moves;
    for (const SelectedBounds& b : bounds) {
        float toX = b.x;
        float toY = b.y;
        switch (op) {
        case AlignOp::Left:
            toX = minLeft;
            break;
        case AlignOp::CenterX:
            toX = centerX - b.width * 0.5f;
            break;
        case AlignOp::Right:
            toX = maxRight - b.width;
            break;
        case AlignOp::Top:
            toY = minTop;
            break;
        case AlignOp::CenterY:
            toY = centerY - b.height * 0.5f;
            break;
        case AlignOp::Bottom:
            toY = maxBottom - b.height;
            break;
        default:
            break;
        }
        AppendIfMoved(moves, graph, b.nodeId, toX, toY);
    }
    return moves;
}

// Spaces nodes so the gaps between adjacent bodies are equal along one
// axis; the two extreme nodes stay put. Needs at least three nodes.
std::vector<NodeMove> Distribute(bool horizontal, std::vector<SelectedBounds> bounds,
                                 const NodeGraph& graph)
{
    if (bounds.size() < 3) {
        return {};
    }
    if (horizontal) {
        std::sort(bounds.begin(), bounds.end(),
                  [](const SelectedBounds& a, const SelectedBounds& b) { return a.x < b.x; });
    } else {
        std::sort(bounds.begin(), bounds.end(),
                  [](const SelectedBounds& a, const SelectedBounds& b) { return a.y < b.y; });
    }

    const SelectedBounds& first = bounds.front();
    const SelectedBounds& last = bounds.back();
    float sumSize = 0.0f;
    for (const SelectedBounds& b : bounds) {
        sumSize += horizontal ? b.width : b.height;
    }
    const float span = horizontal ? (last.x + last.width - first.x)
                                  : (last.y + last.height - first.y);
    const float gap = (span - sumSize) / static_cast<float>(bounds.size() - 1);

    std::vector<NodeMove> moves;
    float cursor = horizontal ? first.x : first.y;
    for (const SelectedBounds& b : bounds) {
        if (horizontal) {
            AppendIfMoved(moves, graph, b.nodeId, cursor, b.y);
            cursor += b.width + gap;
        } else {
            AppendIfMoved(moves, graph, b.nodeId, b.x, cursor);
            cursor += b.height + gap;
        }
    }
    return moves;
}

// Directed adjacency edge for straightening: following it from node "a"
// (whose connected pin sits at pinYa) to node "b", node b must shift so
// its pin at pinYb lines up with a's pin.
struct StraightenEdge
{
    NodeId to = INVALID_ID;
    float pinYFrom = 0.0f;
    float pinYTo = 0.0f;
};

// Moves connected nodes vertically so the wires between selected nodes run
// horizontal. Each connected component is straightened independently,
// keeping its first-visited node fixed.
std::vector<NodeMove> Straighten(const std::vector<SelectedBounds>& bounds,
                                 const NodeGraph& graph, const NodeLayoutCache& layoutCache)
{
    std::unordered_set<NodeId> selectedSet;
    for (const SelectedBounds& b : bounds) {
        selectedSet.insert(b.nodeId);
    }

    std::unordered_map<NodeId, std::vector<StraightenEdge>> adjacency;
    for (const Link& link : graph.GetLinks()) {
        const Pin* fromPin = graph.FindPin(link.fromPinId);
        const Pin* toPin = graph.FindPin(link.toPinId);
        if (fromPin == nullptr || toPin == nullptr) {
            continue;
        }
        if (selectedSet.count(fromPin->nodeId) == 0 || selectedSet.count(toPin->nodeId) == 0) {
            continue;
        }
        const PinLayout* fromLayout = layoutCache.FindPin(link.fromPinId);
        const PinLayout* toLayout = layoutCache.FindPin(link.toPinId);
        if (fromLayout == nullptr || toLayout == nullptr) {
            continue;
        }
        adjacency[fromPin->nodeId].push_back(
            StraightenEdge{toPin->nodeId, fromLayout->y, toLayout->y});
        adjacency[toPin->nodeId].push_back(
            StraightenEdge{fromPin->nodeId, toLayout->y, fromLayout->y});
    }

    std::unordered_map<NodeId, float> deltaY;
    for (const SelectedBounds& b : bounds) {
        if (deltaY.count(b.nodeId) != 0) {
            continue;
        }
        // Breadth-first over one connected component, anchor delta 0.
        deltaY[b.nodeId] = 0.0f;
        std::vector<NodeId> queue{b.nodeId};
        for (std::size_t head = 0; head < queue.size(); ++head) {
            const NodeId current = queue[head];
            const float currentDelta = deltaY[current];
            auto found = adjacency.find(current);
            if (found == adjacency.end()) {
                continue;
            }
            for (const StraightenEdge& edge : found->second) {
                if (deltaY.count(edge.to) != 0) {
                    continue;
                }
                // Want (edge.pinYTo + deltaTo) == (edge.pinYFrom + currentDelta).
                deltaY[edge.to] = edge.pinYFrom + currentDelta - edge.pinYTo;
                queue.push_back(edge.to);
            }
        }
    }

    std::vector<NodeMove> moves;
    for (const SelectedBounds& b : bounds) {
        const float delta = deltaY[b.nodeId];
        if (std::fabs(delta) < MOVE_EPSILON) {
            continue;
        }
        const Node* node = graph.FindNode(b.nodeId);
        if (node != nullptr) {
            moves.push_back(MakeMove(graph, b.nodeId, node->x, node->y + delta));
        }
    }
    return moves;
}

// Horizontal gap between two adjacent layers (columns).
constexpr float LAYOUT_COLUMN_GAP = 80.0f;
// Vertical gap between two stacked nodes in the same layer.
constexpr float LAYOUT_ROW_GAP = 40.0f;

// Assigns each node a layer index via a longest-path pass (Kahn order) so
// that every link points from a lower to a higher layer. Cyclic leftovers
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
                       const std::vector<SelectedBounds>& bounds)
{
    for (std::vector<int>& layerNodes : layers) {
        std::sort(layerNodes.begin(), layerNodes.end(), [&](int a, int b) {
            return bounds[static_cast<std::size_t>(a)].y < bounds[static_cast<std::size_t>(b)].y;
        });
    }

    const int n = static_cast<int>(bounds.size());
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

std::vector<NodeMove> ComputeAutoLayoutMoves(const std::vector<NodeId>& nodesToLayout,
                                             const NodeGraph& graph,
                                             const NodeLayoutCache& layoutCache)
{
    const std::vector<SelectedBounds> bounds = GatherBounds(nodesToLayout, layoutCache);
    const int n = static_cast<int>(bounds.size());
    if (n < 2) {
        return {};
    }

    std::unordered_map<NodeId, int> indexOf;
    for (int i = 0; i < n; ++i) {
        indexOf[bounds[static_cast<std::size_t>(i)].nodeId] = i;
    }

    std::vector<std::vector<int>> succ(static_cast<std::size_t>(n));
    std::vector<std::vector<int>> pred(static_cast<std::size_t>(n));
    std::vector<int> indegree(static_cast<std::size_t>(n), 0);
    for (const Link& link : graph.GetLinks()) {
        const Pin* fromPin = graph.FindPin(link.fromPinId);
        const Pin* toPin = graph.FindPin(link.toPinId);
        if (fromPin == nullptr || toPin == nullptr) {
            continue;
        }
        auto fromIt = indexOf.find(fromPin->nodeId);
        auto toIt = indexOf.find(toPin->nodeId);
        if (fromIt == indexOf.end() || toIt == indexOf.end() || fromIt->second == toIt->second) {
            continue;
        }
        succ[static_cast<std::size_t>(fromIt->second)].push_back(toIt->second);
        pred[static_cast<std::size_t>(toIt->second)].push_back(fromIt->second);
        ++indegree[static_cast<std::size_t>(toIt->second)];
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
    OrderWithinLayers(layers, pred, bounds);

    float originX = bounds[0].x;
    float originY = bounds[0].y;
    for (const SelectedBounds& b : bounds) {
        originX = std::min(originX, b.x);
        originY = std::min(originY, b.y);
    }

    std::vector<float> layerWidth(static_cast<std::size_t>(maxLayer + 1), 0.0f);
    for (int i = 0; i < n; ++i) {
        const std::size_t l = static_cast<std::size_t>(layer[static_cast<std::size_t>(i)]);
        layerWidth[l] = std::max(layerWidth[l], bounds[static_cast<std::size_t>(i)].width);
    }

    std::vector<NodeMove> moves;
    float columnX = originX;
    for (int l = 0; l <= maxLayer; ++l) {
        float rowY = originY;
        for (int nodeIndex : layers[static_cast<std::size_t>(l)]) {
            const SelectedBounds& b = bounds[static_cast<std::size_t>(nodeIndex)];
            AppendIfMoved(moves, graph, b.nodeId, columnX, rowY);
            rowY += b.height + LAYOUT_ROW_GAP;
        }
        columnX += layerWidth[static_cast<std::size_t>(l)] + LAYOUT_COLUMN_GAP;
    }
    return moves;
}

std::vector<NodeMove> ComputeAlignMoves(AlignOp op,
                                        const std::vector<NodeId>& selectedNodes,
                                        const NodeGraph& graph,
                                        const NodeLayoutCache& layoutCache)
{
    const std::vector<SelectedBounds> bounds = GatherBounds(selectedNodes, layoutCache);
    if (bounds.size() < 2) {
        return {};
    }
    switch (op) {
    case AlignOp::Left:
    case AlignOp::CenterX:
    case AlignOp::Right:
    case AlignOp::Top:
    case AlignOp::CenterY:
    case AlignOp::Bottom:
        return AlignEdges(op, bounds, graph);
    case AlignOp::DistributeHorizontal:
        return Distribute(true, bounds, graph);
    case AlignOp::DistributeVertical:
        return Distribute(false, bounds, graph);
    case AlignOp::Straighten:
        return Straighten(bounds, graph, layoutCache);
    }
    return {};
}
