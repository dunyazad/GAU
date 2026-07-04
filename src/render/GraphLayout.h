#pragma once

// Computes canvas-space node and pin rectangles for a v2 Graph. Text
// widths come from an abstract measure function so layout is testable
// without a graphics backend; the renderer and hit test read the result.

#include "model/Graph.h"
#include "model/Ids.h"
#include "model/NodeClassV2.h"

#include "core/Type.h"

#include <functional>
#include <string>
#include <vector>

namespace gau::render {

using MeasureTextFn = std::function<float(const std::string&, float size)>;

struct PinLayout
{
    PinId id = INVALID_ID;
    TypeId type = INVALID_TYPE;
    PinDirection direction = PinDirection::Input;
    float x = 0.0f;
    float y = 0.0f;
};

struct NodeLayout
{
    NodeId id = INVALID_ID;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::vector<PinLayout> pins;
};

class GraphLayout
{
public:
    void Add(NodeLayout layout) { nodes.push_back(std::move(layout)); }
    const std::vector<NodeLayout>& Nodes() const { return nodes; }
    const NodeLayout* FindNode(NodeId id) const;
    const PinLayout* FindPin(PinId id) const;

private:
    std::vector<NodeLayout> nodes;
};

GraphLayout ComputeGraphLayout(const Graph& graph, const NodeClassRegistry& classes,
                               const MeasureTextFn& measure);

} // namespace gau::render
