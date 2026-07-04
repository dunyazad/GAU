#include "PropertyPanelRenderer.h"

#include "interaction/PropertyPanel.h"
#include "model/NodeGraph.h"
#include "model/PropertyText.h"
#include "model/UserType.h"

#include <nanovg.h>

#include <string>
#include <variant>

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

// Display text of one struct field value: enumerator name for enum fields,
// "{...}" for nested structs, else the scalar text.
static std::string StructFieldValueText(const PropertyDef& def, const PropertyValue& propValue,
                                        int fieldIndex)
{
    const UserType* structType = UserTypeRegistry::Find(def.typeName);
    if (structType == nullptr || fieldIndex >= static_cast<int>(structType->fields.size())) {
        return "";
    }
    const StructField& field = structType->fields[static_cast<std::size_t>(fieldIndex)];
    const PropertyValue empty;
    const PropertyValue& fieldValue =
        (fieldIndex < static_cast<int>(propValue.structFields.size()))
            ? propValue.structFields[static_cast<std::size_t>(fieldIndex)]
            : empty;
    if (field.type == PinType::UserType) {
        const UserType* fieldType = UserTypeRegistry::Find(field.typeName);
        if (fieldType != nullptr && fieldType->kind == UserTypeKind::Struct) {
            return "{...}";
        }
        if (fieldType != nullptr && fieldType->kind == UserTypeKind::Enum) {
            const int* index = std::get_if<int>(&fieldValue.scalar);
            if (index != nullptr && *index >= 0
                && *index < static_cast<int>(fieldType->enumerators.size())) {
                return fieldType->enumerators[static_cast<std::size_t>(*index)];
            }
            return "0";
        }
    }
    return ValueToString(fieldValue.scalar);
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

    const std::vector<PropertyRow> rows = panel.BuildRows(&node);
    for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
        const PropertyRow& row = rows[static_cast<std::size_t>(r)];
        const PropertyDef& def = defs[static_cast<std::size_t>(row.propertyIndex)];
        const PropertyValue empty;
        const PropertyValue& propValue =
            (row.propertyIndex < static_cast<int>(node.propertyValues.size()))
                ? node.propertyValues[static_cast<std::size_t>(row.propertyIndex)]
                : empty;
        const UIRect field = panel.FieldRectForDepth(r, row.depth, screenWidth);
        const float labelX = rect.x + PropertyPanel::PADDING
                           + static_cast<float>(row.depth) * 12.0f * UI_SCALE;

        // Label.
        nvgFillColor(vg, nvgRGB(170, 170, 178));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgSave(vg);
        nvgIntersectScissor(vg, labelX, field.y, PropertyPanel::LABEL_WIDTH - 4.0f * UI_SCALE,
                            field.h);
        std::string label;
        if (row.fieldIndex >= 0) {
            const UserType* structType = UserTypeRegistry::Find(def.typeName);
            if (structType != nullptr
                && row.fieldIndex < static_cast<int>(structType->fields.size())) {
                label = structType->fields[static_cast<std::size_t>(row.fieldIndex)].name;
            }
        } else {
            label = def.name + ContainerTag(def.container);
        }
        nvgText(vg, labelX, field.y + field.h * 0.5f, label.c_str(), nullptr);
        nvgRestore(vg);

        if (row.isHeader) {
            // Struct header: name only, no editable field box.
            continue;
        }

        const bool focused = panel.GetFocusedProperty() == row.propertyIndex
                          && panel.GetFocusedField() == row.fieldIndex;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, field.x, field.y, field.w, field.h, 3.0f * UI_SCALE);
        nvgFillColor(vg, nvgRGB(15, 15, 17));
        nvgFill(vg);
        nvgStrokeColor(vg, focused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
        nvgStrokeWidth(vg, 1.0f);
        nvgStroke(vg);

        std::string shown;
        if (focused) {
            shown = panel.GetEditText() + "|";
        } else if (row.fieldIndex >= 0) {
            shown = StructFieldValueText(def, propValue, row.fieldIndex);
        } else {
            shown = PropertyValueToText(def, propValue);
        }
        nvgSave(vg);
        nvgIntersectScissor(vg, field.x, field.y, field.w, field.h);
        nvgFillColor(vg, nvgRGB(235, 235, 240));
        nvgText(vg, field.x + 6.0f * UI_SCALE, field.y + field.h * 0.5f, shown.c_str(), nullptr);
        nvgRestore(vg);
    }
}
