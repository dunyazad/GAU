#include "PropertyPanelRenderer.h"

#include "interaction/PropertyPanel.h"
#include "model/NodeGraph.h"
#include "model/PropertyText.h"

#include <nanovg.h>

#include <string>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const float PANEL_FONT_SIZE = 13.0f * UI_SCALE;

static const char* ContainerTag(PropertyContainer container)
{
    switch (container) {
    case PropertyContainer::None:
        return "";
    case PropertyContainer::Array:
        return " []";
    case PropertyContainer::Set:
        return " {}";
    case PropertyContainer::Map:
        return " {:}";
    }
    return "";
}

void DrawPropertyPanel(NVGcontext* vg, const PropertyPanel& panel, const Node& node,
                       float screenWidth)
{
    const UIRect rect = panel.PanelRect(&node, screenWidth);

    // Panel background.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, panel.IsDocked() ? 0.0f : 4.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(22, 22, 25, 245));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Header (drag handle).
    const UIRect header = panel.HeaderRect(screenWidth);
    nvgBeginPath(vg);
    nvgRect(vg, header.x, header.y, header.w, header.h);
    nvgFillColor(vg, nvgRGB(32, 32, 38));
    nvgFill(vg);

    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, PANEL_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(235, 235, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    const std::string title = std::string(node.nodeClass->GetName()) + " Properties";
    nvgSave(vg);
    nvgIntersectScissor(vg, header.x, header.y, header.w, header.h);
    nvgText(vg, header.x + PropertyPanel::PADDING, header.y + header.h * 0.5f,
            title.c_str(), nullptr);
    nvgRestore(vg);

    const std::vector<PropertyDef>& defs = node.nodeClass->GetProperties();

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, PANEL_FONT_SIZE);

    if (defs.empty()) {
        nvgFillColor(vg, nvgRGB(120, 120, 128));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, rect.x + PropertyPanel::PADDING,
                header.y + header.h + PropertyPanel::PADDING
                    + PropertyPanel::ROW_STRIDE * 0.5f,
                "No properties", nullptr);
        return;
    }

    for (int i = 0; i < static_cast<int>(defs.size()); ++i) {
        const PropertyDef& def = defs[static_cast<std::size_t>(i)];
        const UIRect field = panel.FieldRect(i, screenWidth);

        // Label.
        nvgFillColor(vg, nvgRGB(170, 170, 178));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgSave(vg);
        nvgIntersectScissor(vg, rect.x + PropertyPanel::PADDING, field.y,
                            PropertyPanel::LABEL_WIDTH - 4.0f * UI_SCALE, field.h);
        const std::string label = def.name + ContainerTag(def.container);
        nvgText(vg, rect.x + PropertyPanel::PADDING, field.y + field.h * 0.5f,
                label.c_str(), nullptr);
        nvgRestore(vg);

        // Field.
        const bool focused = (panel.GetFocusedProperty() == i);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, field.x, field.y, field.w, field.h, 3.0f * UI_SCALE);
        nvgFillColor(vg, nvgRGB(15, 15, 17));
        nvgFill(vg);
        nvgStrokeColor(vg, focused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        const PropertyValue& value = (i < static_cast<int>(node.propertyValues.size()))
                                         ? node.propertyValues[static_cast<std::size_t>(i)]
                                         : PropertyValue();
        const std::string shown = focused ? panel.GetEditText() + "|"
                                          : PropertyValueToText(def, value);
        nvgSave(vg);
        nvgIntersectScissor(vg, field.x, field.y, field.w, field.h);
        nvgFillColor(vg, nvgRGB(235, 235, 240));
        nvgText(vg, field.x + 6.0f * UI_SCALE, field.y + field.h * 0.5f,
                shown.c_str(), nullptr);
        nvgRestore(vg);
    }
}
