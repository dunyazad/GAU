#include "NodeRenderer.h"
#include "CategoryStyle.h"
#include "PinStyle.h"

#include "model/NodeGraph.h"

#include <nanovg.h>

#include <algorithm>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const float TITLE_FONT_SIZE = 14.0f;
static const float PIN_LABEL_FONT_SIZE = 13.0f;
static const float PROPERTY_FONT_SIZE = 12.0f;
static const float PIN_LABEL_GAP = 10.0f;
static const float PIN_COLUMN_GAP = 40.0f;

// Display line for one property on the node body:
// scalar "name: v", array "name: [a, b]", set "name: {a, b}",
// map "name: {k: v, k2: v2}".
// When a scalar property has a same-named input pin that is linked,
// returns the evaluated source value ("(linked)" if the source value is
// not known without a full run). Returns empty when no such live value
// applies, so the caller falls back to the static property.
static std::string LinkedScalarDisplay(const Node& node, const std::string& propertyName,
                                       const NodeGraph& graph, const PinValueCache& previewValues)
{
    for (const Pin& inputPin : node.inputs) {
        if (inputPin.type == PinType::Exec || inputPin.name != propertyName) {
            continue;
        }
        const Link* link = graph.FindLinkToInput(inputPin.id);
        if (link == nullptr) {
            return std::string();
        }
        for (const std::pair<PinId, Value>& entry : previewValues) {
            if (entry.first == link->fromPinId) {
                return propertyName + ": " + ValueToString(entry.second);
            }
        }
        return propertyName + ": (linked)";
    }
    return std::string();
}

static std::string PropertyDisplayText(const Node& node, int propertyIndex,
                                       const NodeGraph& graph, const PinValueCache& previewValues)
{
    const PropertyDef& def = node.nodeClass->GetProperties()[static_cast<std::size_t>(propertyIndex)];

    PropertyValue fallback;
    fallback.scalar = def.defaultValue;
    fallback.elements = def.defaultElements;
    fallback.entries = def.defaultEntries;
    const PropertyValue& value = (propertyIndex < static_cast<int>(node.propertyValues.size()))
                                     ? node.propertyValues[static_cast<std::size_t>(propertyIndex)]
                                     : fallback;

    switch (def.container) {
    case PropertyContainer::None: {
        const std::string linked = LinkedScalarDisplay(node, def.name, graph, previewValues);
        if (!linked.empty()) {
            return linked;
        }
        return def.name + ": " + ValueToString(value.scalar);
    }

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        std::string joined;
        for (const Value& element : value.elements) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(element);
        }
        if (def.container == PropertyContainer::Array) {
            return def.name + ": [" + joined + "]";
        }
        return def.name + ": {" + joined + "}";
    }

    case PropertyContainer::Map: {
        std::string joined;
        for (const std::pair<Value, Value>& entry : value.entries) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(entry.first) + ": " + ValueToString(entry.second);
        }
        return def.name + ": {" + joined + "}";
    }
    }
    return def.name;
}

// Y offset (from node top) where property rows begin.
static float PropertyRowsTop(int pinRowCount)
{
    return NODE_HEADER_HEIGHT + NODE_BODY_PADDING
         + NODE_PIN_ROW_HEIGHT * static_cast<float>(pinRowCount)
         + NODE_PROPERTY_SECTION_GAP;
}

static float MeasureTextWidth(NVGcontext* vg, const char* fontFace, float fontSize, const char* text)
{
    if (text == nullptr || text[0] == '\0') {
        return 0.0f;
    }
    nvgFontFace(vg, fontFace);
    nvgFontSize(vg, fontSize);
    float bounds[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    nvgTextBounds(vg, 0.0f, 0.0f, text, nullptr, bounds);
    return bounds[2] - bounds[0];
}

static float PinRowCenterY(int rowIndex)
{
    return NODE_HEADER_HEIGHT + NODE_BODY_PADDING
         + NODE_PIN_ROW_HEIGHT * static_cast<float>(rowIndex)
         + NODE_PIN_ROW_HEIGHT * 0.5f;
}

NodeLayout ComputeNodeLayout(NVGcontext* vg, const Node& node,
                             const NodeGraph& graph, const PinValueCache& previewValues)
{
    NodeLayout layout;
    layout.nodeId = node.id;
    layout.x = node.x;
    layout.y = node.y;

    const int inputCount = static_cast<int>(node.inputs.size());
    const int outputCount = static_cast<int>(node.outputs.size());
    const int rowCount = std::max(inputCount, outputCount);

    const float titleWidth = MeasureTextWidth(vg, FONT_BOLD, TITLE_FONT_SIZE, node.nodeClass->GetName());
    float width = std::max(NODE_MIN_WIDTH, titleWidth + 24.0f);

    for (int row = 0; row < rowCount; ++row) {
        float inputLabelWidth = 0.0f;
        float outputLabelWidth = 0.0f;
        if (row < inputCount) {
            inputLabelWidth = MeasureTextWidth(vg, FONT_REGULAR, PIN_LABEL_FONT_SIZE,
                                               node.inputs[static_cast<std::size_t>(row)].name.c_str());
        }
        if (row < outputCount) {
            outputLabelWidth = MeasureTextWidth(vg, FONT_REGULAR, PIN_LABEL_FONT_SIZE,
                                                node.outputs[static_cast<std::size_t>(row)].name.c_str());
        }
        const float rowWidth = NODE_PIN_INSET + PIN_LABEL_GAP + inputLabelWidth
                             + PIN_COLUMN_GAP
                             + outputLabelWidth + PIN_LABEL_GAP + NODE_PIN_INSET;
        width = std::max(width, rowWidth);
    }

    const int propertyCount = static_cast<int>(node.nodeClass->GetProperties().size());
    for (int i = 0; i < propertyCount; ++i) {
        const std::string text = PropertyDisplayText(node, i, graph, previewValues);
        const float textWidth = MeasureTextWidth(vg, FONT_REGULAR, PROPERTY_FONT_SIZE, text.c_str());
        width = std::max(width, NODE_PIN_INSET * 2.0f + textWidth);
    }

    layout.width = width;
    layout.height = NODE_HEADER_HEIGHT + NODE_BODY_PADDING
                  + NODE_PIN_ROW_HEIGHT * static_cast<float>(rowCount)
                  + NODE_BODY_PADDING;
    if (propertyCount > 0) {
        layout.height += NODE_PROPERTY_SECTION_GAP
                       + NODE_PROPERTY_ROW_HEIGHT * static_cast<float>(propertyCount);
    }

    for (int row = 0; row < inputCount; ++row) {
        const Pin& pin = node.inputs[static_cast<std::size_t>(row)];
        PinLayout pinLayout;
        pinLayout.pinId = pin.id;
        pinLayout.direction = PinDirection::Input;
        pinLayout.type = pin.type;
        pinLayout.x = node.x + NODE_PIN_INSET;
        pinLayout.y = node.y + PinRowCenterY(row);
        layout.pins.push_back(pinLayout);
    }
    for (int row = 0; row < outputCount; ++row) {
        const Pin& pin = node.outputs[static_cast<std::size_t>(row)];
        PinLayout pinLayout;
        pinLayout.pinId = pin.id;
        pinLayout.direction = PinDirection::Output;
        pinLayout.type = pin.type;
        pinLayout.x = node.x + layout.width - NODE_PIN_INSET;
        pinLayout.y = node.y + PinRowCenterY(row);
        layout.pins.push_back(pinLayout);
    }

    return layout;
}

static void DrawExecPin(NVGcontext* vg, float cx, float cy, bool connected)
{
    nvgBeginPath(vg);
    nvgMoveTo(vg, cx - 5.0f, cy - 6.0f);
    nvgLineTo(vg, cx + 1.0f, cy - 6.0f);
    nvgLineTo(vg, cx + 6.0f, cy);
    nvgLineTo(vg, cx + 1.0f, cy + 6.0f);
    nvgLineTo(vg, cx - 5.0f, cy + 6.0f);
    nvgClosePath(vg);

    if (connected) {
        nvgFillColor(vg, nvgRGB(255, 255, 255));
        nvgFill(vg);
    } else {
        nvgStrokeColor(vg, nvgRGB(255, 255, 255));
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }
}

static void DrawDataPin(NVGcontext* vg, float cx, float cy, PinType type, bool connected)
{
    const NVGcolor color = PinColorForType(type);

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, DATA_PIN_RADIUS);
    if (connected) {
        nvgFillColor(vg, color);
        nvgFill(vg);
    } else {
        nvgStrokeColor(vg, color);
        nvgStrokeWidth(vg, 1.5f);
        nvgStroke(vg);
    }
}

static void DrawPinLabel(NVGcontext* vg, const PinLayout& pin, const char* label)
{
    if (label == nullptr || label[0] == '\0') {
        return;
    }
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, PIN_LABEL_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(200, 200, 205));
    if (pin.direction == PinDirection::Input) {
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, pin.x + PIN_LABEL_GAP, pin.y, label, nullptr);
    } else {
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        nvgText(vg, pin.x - PIN_LABEL_GAP, pin.y, label, nullptr);
    }
}

static const Pin* FindPinInNode(const Node& node, PinId pinId)
{
    for (const Pin& pin : node.inputs) {
        if (pin.id == pinId) {
            return &pin;
        }
    }
    for (const Pin& pin : node.outputs) {
        if (pin.id == pinId) {
            return &pin;
        }
    }
    return nullptr;
}

void DrawNode(NVGcontext* vg, const Node& node, const NodeLayout& layout, bool selected,
              const NodeGraph& graph, const PinValueCache& previewValues)
{
    const float x = layout.x;
    const float y = layout.y;
    const float w = layout.width;
    const float h = layout.height;

    // Body.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, NODE_CORNER_RADIUS);
    nvgFillColor(vg, nvgRGBA(24, 24, 28, 235));
    nvgFill(vg);

    // Header with a left-to-right gradient: base color -> base * 0.4.
    const NVGcolor headerBase = CategoryColor(node.nodeClass->GetCategory());
    const NVGcolor headerDark = nvgRGBf(headerBase.r * 0.4f, headerBase.g * 0.4f, headerBase.b * 0.4f);
    const NVGpaint headerPaint = nvgLinearGradient(vg, x, y, x + w, y, headerBase, headerDark);
    nvgBeginPath(vg);
    nvgRoundedRectVarying(vg, x, y, w, NODE_HEADER_HEIGHT,
                          NODE_CORNER_RADIUS, NODE_CORNER_RADIUS, 0.0f, 0.0f);
    nvgFillPaint(vg, headerPaint);
    nvgFill(vg);

    // Title.
    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, TITLE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(240, 240, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + 10.0f, y + NODE_HEADER_HEIGHT * 0.5f, node.nodeClass->GetName(), nullptr);

    // Border (selection state uses the spec highlight color).
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, NODE_CORNER_RADIUS);
    if (selected) {
        nvgStrokeColor(vg, nvgRGB(255, 180, 40));
        nvgStrokeWidth(vg, 2.0f);
    } else {
        nvgStrokeColor(vg, nvgRGB(60, 60, 66));
        nvgStrokeWidth(vg, 1.0f);
    }
    nvgStroke(vg);

    // Pins and labels.
    for (const PinLayout& pinLayout : layout.pins) {
        if (pinLayout.type == PinType::Exec) {
            DrawExecPin(vg, pinLayout.x, pinLayout.y, pinLayout.connected);
        } else {
            DrawDataPin(vg, pinLayout.x, pinLayout.y, pinLayout.type, pinLayout.connected);
        }
        const Pin* pin = FindPinInNode(node, pinLayout.pinId);
        if (pin != nullptr) {
            DrawPinLabel(vg, pinLayout, pin->name.c_str());
        }
    }

    // Property rows below the pins.
    const int propertyCount = static_cast<int>(node.nodeClass->GetProperties().size());
    if (propertyCount > 0) {
        const int pinRowCount = std::max(static_cast<int>(node.inputs.size()),
                                         static_cast<int>(node.outputs.size()));
        const float propertyTop = y + PropertyRowsTop(pinRowCount);

        nvgFontFace(vg, FONT_REGULAR);
        nvgFontSize(vg, PROPERTY_FONT_SIZE);
        nvgFillColor(vg, nvgRGB(160, 160, 170));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < propertyCount; ++i) {
            const std::string text = PropertyDisplayText(node, i, graph, previewValues);
            const float rowCenterY = propertyTop
                                   + NODE_PROPERTY_ROW_HEIGHT * (static_cast<float>(i) + 0.5f);
            nvgText(vg, x + NODE_PIN_INSET, rowCenterY, text.c_str(), nullptr);
        }
    }
}
