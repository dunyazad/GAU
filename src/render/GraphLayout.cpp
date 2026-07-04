// v2 graph layout computation.

#include "GraphLayout.h"

#include <algorithm>

namespace gau::render {

static const float NODE_MIN_WIDTH = 160.0f;
static const float HEADER_HEIGHT = 26.0f;
static const float BODY_PADDING = 8.0f;
static const float PIN_ROW_HEIGHT = 22.0f;
static const float PIN_INSET = 12.0f;
static const float PIN_LABEL_GAP = 8.0f;
static const float COLUMN_GAP = 24.0f;
static const float TITLE_FONT = 14.0f;
static const float PIN_FONT = 12.0f;

const NodeLayout* GraphLayout::FindNode(NodeId id) const
{
    for (const NodeLayout& node : nodes) {
        if (node.id == id) {
            return &node;
        }
    }
    return nullptr;
}

const PinLayout* GraphLayout::FindPin(PinId id) const
{
    for (const NodeLayout& node : nodes) {
        for (const PinLayout& pin : node.pins) {
            if (pin.id == id) {
                return &pin;
            }
        }
    }
    return nullptr;
}

static float NodeWidth(const Node& node, const MeasureTextFn& measure)
{
    float width = std::max(NODE_MIN_WIDTH, measure(node.className, TITLE_FONT) + 24.0f);
    const std::size_t rows = std::max(node.inputs.size(), node.outputs.size());
    for (std::size_t row = 0; row < rows; ++row) {
        float inLabel = 0.0f;
        float outLabel = 0.0f;
        if (row < node.inputs.size()) {
            inLabel = measure(node.inputs[row].name, PIN_FONT);
        }
        if (row < node.outputs.size()) {
            outLabel = measure(node.outputs[row].name, PIN_FONT);
        }
        const float rowWidth = PIN_INSET + PIN_LABEL_GAP + inLabel + COLUMN_GAP + outLabel
                             + PIN_LABEL_GAP + PIN_INSET;
        width = std::max(width, rowWidth);
    }
    return width;
}

static float PinRowCenterY(const Node& node, int row)
{
    return node.y + HEADER_HEIGHT + BODY_PADDING + PIN_ROW_HEIGHT * (static_cast<float>(row) + 0.5f);
}

GraphLayout ComputeGraphLayout(const Graph& graph, const NodeClassRegistry& classes,
                               const MeasureTextFn& measure)
{
    (void)classes;
    GraphLayout layout;
    for (const Node& node : graph.Nodes()) {
        NodeLayout nl;
        nl.id = node.id;
        nl.x = node.x;
        nl.y = node.y;
        nl.w = NodeWidth(node, measure);
        const std::size_t rows = std::max(node.inputs.size(), node.outputs.size());
        nl.h = HEADER_HEIGHT + BODY_PADDING + PIN_ROW_HEIGHT * static_cast<float>(rows)
             + BODY_PADDING;

        for (std::size_t i = 0; i < node.inputs.size(); ++i) {
            PinLayout pin;
            pin.id = node.inputs[i].id;
            pin.type = node.inputs[i].type;
            pin.direction = PinDirection::Input;
            pin.x = node.x + PIN_INSET;
            pin.y = PinRowCenterY(node, static_cast<int>(i));
            nl.pins.push_back(pin);
        }
        for (std::size_t i = 0; i < node.outputs.size(); ++i) {
            PinLayout pin;
            pin.id = node.outputs[i].id;
            pin.type = node.outputs[i].type;
            pin.direction = PinDirection::Output;
            pin.x = node.x + nl.w - PIN_INSET;
            pin.y = PinRowCenterY(node, static_cast<int>(i));
            nl.pins.push_back(pin);
        }
        layout.Add(std::move(nl));
    }
    return layout;
}

} // namespace gau::render
