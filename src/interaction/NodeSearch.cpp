// Node search and focus-bounds math.

#include "NodeSearch.h"

#include <algorithm>
#include <cctype>

namespace gau {

static std::string ToLower(const std::string& text)
{
    std::string out = text;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

std::vector<NodeId> SearchNodes(const Graph& graph, const std::string& query)
{
    std::vector<NodeId> matches;
    if (query.empty()) {
        return matches;
    }
    const std::string needle = ToLower(query);
    for (const Node& node : graph.Nodes()) {
        if (ToLower(node.className).find(needle) != std::string::npos) {
            matches.push_back(node.id);
        }
    }
    return matches;
}

bool ComputeBounds(const std::vector<NodeBox>& boxes, Bounds& out)
{
    if (boxes.empty()) {
        return false;
    }
    out.minX = boxes[0].x;
    out.minY = boxes[0].y;
    out.maxX = boxes[0].x + boxes[0].w;
    out.maxY = boxes[0].y + boxes[0].h;
    for (const NodeBox& b : boxes) {
        out.minX = std::min(out.minX, b.x);
        out.minY = std::min(out.minY, b.y);
        out.maxX = std::max(out.maxX, b.x + b.w);
        out.maxY = std::max(out.maxY, b.y + b.h);
    }
    return true;
}

} // namespace gau
