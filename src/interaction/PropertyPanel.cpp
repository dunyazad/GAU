#include "PropertyPanel.h"

#include "model/PropertyText.h"
#include "model/UserType.h"

#include <string>
#include <variant>

static void PopLastUTF8Character(std::string& text)
{
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

void PropertyPanel::SetTarget(const Node* node)
{
    const NodeId newTargetId = (node != nullptr) ? node->id : INVALID_ID;
    if (newTargetId != targetNodeId) {
        targetNodeId = newTargetId;
        focusedProperty = -1;
        focusedField = -1;
        editText.clear();
    }
}

std::vector<PropertyRow> PropertyPanel::BuildRows(const Node* node) const
{
    std::vector<PropertyRow> rows;
    if (node == nullptr) {
        return rows;
    }
    const std::vector<PropertyDef>& defs = node->nodeClass->GetProperties();
    for (int i = 0; i < static_cast<int>(defs.size()); ++i) {
        const PropertyDef& def = defs[static_cast<std::size_t>(i)];
        const UserType* structType = nullptr;
        if (def.container == PropertyContainer::None && def.type == PinType::UserType) {
            const UserType* userType = UserTypeRegistry::Find(def.typeName);
            if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
                structType = userType;
            }
        }
        if (structType != nullptr) {
            PropertyRow header;
            header.propertyIndex = i;
            header.fieldIndex = -1;
            header.isHeader = true;
            header.depth = 0;
            rows.push_back(header);
            for (int f = 0; f < static_cast<int>(structType->fields.size()); ++f) {
                PropertyRow fieldRow;
                fieldRow.propertyIndex = i;
                fieldRow.fieldIndex = f;
                fieldRow.depth = 1;
                rows.push_back(fieldRow);
            }
        } else {
            PropertyRow row;
            row.propertyIndex = i;
            row.fieldIndex = -1;
            row.depth = 0;
            rows.push_back(row);
        }
    }
    return rows;
}

float PropertyPanel::GetX(float screenWidth) const
{
    return docked ? screenWidth - WIDTH : floatX;
}

float PropertyPanel::GetY() const
{
    return docked ? TOP_OFFSET : floatY;
}

float PropertyPanel::GetPanelHeight(const Node* node) const
{
    const int rowCount = static_cast<int>(BuildRows(node).size());
    const float listHeight = (rowCount > 0) ? ROW_STRIDE * static_cast<float>(rowCount)
                                            : ROW_STRIDE;
    return HEADER_HEIGHT + PADDING + listHeight + PADDING;
}

UIRect PropertyPanel::PanelRect(const Node* node, float screenWidth) const
{
    return UIRect{GetX(screenWidth), GetY(), WIDTH, GetPanelHeight(node)};
}

UIRect PropertyPanel::HeaderRect(float screenWidth) const
{
    return UIRect{GetX(screenWidth), GetY(), WIDTH, HEADER_HEIGHT};
}

UIRect PropertyPanel::FieldRect(int rowIndex, float screenWidth) const
{
    return FieldRectForDepth(rowIndex, 0, screenWidth);
}

UIRect PropertyPanel::FieldRectForDepth(int rowIndex, int depth, float screenWidth) const
{
    const float rowY = GetY() + HEADER_HEIGHT + PADDING
                     + ROW_STRIDE * static_cast<float>(rowIndex);
    const float indent = static_cast<float>(depth) * 12.0f * UI_SCALE;
    const float fieldX = GetX(screenWidth) + PADDING + LABEL_WIDTH + indent;
    return UIRect{fieldX, rowY, WIDTH - PADDING * 2.0f - LABEL_WIDTH - indent, ROW_HEIGHT};
}

void PropertyPanel::CommitFocus(PropertyPanelAction& outAction)
{
    if (focusedProperty >= 0) {
        outAction.type = PropertyPanelAction::Type::SetProperty;
        outAction.propertyIndex = focusedProperty;
        outAction.fieldIndex = focusedField;
        outAction.text = editText;
    }
    focusedProperty = -1;
    focusedField = -1;
    editText.clear();
}

bool PropertyPanel::EditRowOnClick(const Node& node, const std::vector<PropertyDef>& defs,
                                   const PropertyRow& row, PropertyPanelAction& outAction)
{
    const PropertyDef& def = defs[static_cast<std::size_t>(row.propertyIndex)];
    const PropertyValue empty;
    const PropertyValue& propValue =
        (row.propertyIndex < static_cast<int>(node.propertyValues.size()))
            ? node.propertyValues[static_cast<std::size_t>(row.propertyIndex)]
            : empty;

    PinType type = def.type;
    std::string typeName = def.typeName;
    PropertyContainer container = def.container;
    Value scalar = propValue.scalar;
    std::string currentText = PropertyValueToText(def, propValue);

    if (row.fieldIndex >= 0) {
        const UserType* structType = UserTypeRegistry::Find(def.typeName);
        if (structType == nullptr
            || row.fieldIndex >= static_cast<int>(structType->fields.size())) {
            return false;
        }
        const StructField& field = structType->fields[static_cast<std::size_t>(row.fieldIndex)];
        type = field.type;
        typeName = field.typeName;
        container = PropertyContainer::None;
        const PropertyValue& fieldValue =
            (row.fieldIndex < static_cast<int>(propValue.structFields.size()))
                ? propValue.structFields[static_cast<std::size_t>(row.fieldIndex)]
                : empty;
        scalar = fieldValue.scalar;
        currentText = ValueToString(fieldValue.scalar);
    }

    // Nested struct field is not editable inline.
    if (container == PropertyContainer::None && type == PinType::UserType) {
        const UserType* userType = UserTypeRegistry::Find(typeName);
        if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
            return false;
        }
    }

    if (container == PropertyContainer::None && type == PinType::Bool) {
        const bool* boolValue = std::get_if<bool>(&scalar);
        outAction.type = PropertyPanelAction::Type::SetProperty;
        outAction.propertyIndex = row.propertyIndex;
        outAction.fieldIndex = row.fieldIndex;
        outAction.text = (boolValue != nullptr && *boolValue) ? "false" : "true";
        return false;
    }

    if (container == PropertyContainer::None && type == PinType::UserType) {
        const UserType* enumType = UserTypeRegistry::Find(typeName);
        if (enumType != nullptr && enumType->kind == UserTypeKind::Enum
            && !enumType->enumerators.empty()) {
            const int* indexPtr = std::get_if<int>(&scalar);
            const int current = (indexPtr != nullptr) ? *indexPtr : 0;
            const int count = static_cast<int>(enumType->enumerators.size());
            const int next = ((current % count) + count + 1) % count;
            outAction.type = PropertyPanelAction::Type::SetProperty;
            outAction.propertyIndex = row.propertyIndex;
            outAction.fieldIndex = row.fieldIndex;
            outAction.text = std::to_string(next);
            return false;
        }
    }

    editText = currentText;
    return true;
}

bool PropertyPanel::HandleEvent(const EditorInputEvent& event, const Node* node,
                                float screenWidth, float screenHeight,
                                PropertyPanelAction& outAction)
{
    if (node == nullptr) {
        return false;
    }

    switch (event.type) {
    case EditorInputType::MouseDown: {
        if (event.button != EditorMouseButton::Left) {
            return PanelRect(node, screenWidth).Contains(event.x, event.y);
        }
        if (HeaderRect(screenWidth).Contains(event.x, event.y)) {
            CommitFocus(outAction);
            draggingHeader = true;
            const UIRect header = HeaderRect(screenWidth);
            dragOffsetX = event.x - header.x;
            dragOffsetY = event.y - header.y;
            return true;
        }
        if (PanelRect(node, screenWidth).Contains(event.x, event.y)) {
            const std::vector<PropertyRow> rows = BuildRows(node);
            const std::vector<PropertyDef>& defs = node->nodeClass->GetProperties();
            for (int r = 0; r < static_cast<int>(rows.size()); ++r) {
                const PropertyRow& row = rows[static_cast<std::size_t>(r)];
                if (row.isHeader
                    || !FieldRectForDepth(r, row.depth, screenWidth).Contains(event.x, event.y)) {
                    continue;
                }
                CommitFocus(outAction);
                if (EditRowOnClick(*node, defs, row, outAction)) {
                    focusedProperty = row.propertyIndex;
                    focusedField = row.fieldIndex;
                }
                return true;
            }
            CommitFocus(outAction);
            return true;
        }
        // Click outside: commit the focused field, let the event pass.
        CommitFocus(outAction);
        return false;
    }

    case EditorInputType::MouseMove:
        if (draggingHeader) {
            docked = false;
            floatX = event.x - dragOffsetX;
            floatY = event.y - dragOffsetY;
            return true;
        }
        return false;

    case EditorInputType::MouseUp:
        if (draggingHeader) {
            draggingHeader = false;
            if (floatX + WIDTH > screenWidth - DOCK_SNAP) {
                docked = true;
            }
            if (!docked) {
                if (floatY < TOP_OFFSET) {
                    floatY = TOP_OFFSET;
                }
                if (floatY > screenHeight - HEADER_HEIGHT) {
                    floatY = screenHeight - HEADER_HEIGHT;
                }
            }
            return true;
        }
        return false;

    case EditorInputType::KeyDown:
        if (focusedProperty < 0) {
            return false;
        }
        if (event.key == EditorKey::Enter) {
            CommitFocus(outAction);
        } else if (event.key == EditorKey::Escape) {
            focusedProperty = -1;
            editText.clear();
        } else if (event.key == EditorKey::Backspace) {
            PopLastUTF8Character(editText);
        }
        return true;

    case EditorInputType::TextInput:
        if (focusedProperty < 0) {
            return false;
        }
        if (editText.size() < 96) {
            editText += event.text;
        }
        return true;

    case EditorInputType::MouseWheel:
        return PanelRect(node, screenWidth).Contains(event.x, event.y);
    }
    return false;
}
