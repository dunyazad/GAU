#include "PropertyPanel.h"

#include "model/PropertyText.h"

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
        editText.clear();
    }
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
    const int propertyCount =
        (node != nullptr) ? static_cast<int>(node->nodeClass->GetProperties().size()) : 0;
    const float listHeight = (propertyCount > 0)
                                 ? ROW_STRIDE * static_cast<float>(propertyCount)
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

UIRect PropertyPanel::FieldRect(int propertyIndex, float screenWidth) const
{
    const float rowY = GetY() + HEADER_HEIGHT + PADDING
                     + ROW_STRIDE * static_cast<float>(propertyIndex);
    const float fieldX = GetX(screenWidth) + PADDING + LABEL_WIDTH;
    return UIRect{fieldX, rowY, WIDTH - PADDING * 2.0f - LABEL_WIDTH, ROW_HEIGHT};
}

void PropertyPanel::CommitFocus(PropertyPanelAction& outAction)
{
    if (focusedProperty >= 0) {
        outAction.type = PropertyPanelAction::Type::SetProperty;
        outAction.propertyIndex = focusedProperty;
        outAction.text = editText;
    }
    focusedProperty = -1;
    editText.clear();
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
            const int propertyCount =
                static_cast<int>(node->nodeClass->GetProperties().size());
            for (int i = 0; i < propertyCount; ++i) {
                if (FieldRect(i, screenWidth).Contains(event.x, event.y)) {
                    CommitFocus(outAction);
                    const PropertyDef& def =
                        node->nodeClass->GetProperties()[static_cast<std::size_t>(i)];
                    const PropertyValue& value =
                        (i < static_cast<int>(node->propertyValues.size()))
                            ? node->propertyValues[static_cast<std::size_t>(i)]
                            : PropertyValue();
                    if (def.container == PropertyContainer::None
                        && def.type == PinType::Bool) {
                        // Bool scalars toggle directly.
                        outAction.type = PropertyPanelAction::Type::SetProperty;
                        outAction.propertyIndex = i;
                        outAction.text = std::get<bool>(value.scalar) ? "false" : "true";
                    } else {
                        focusedProperty = i;
                        editText = PropertyValueToText(def, value);
                    }
                    return true;
                }
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
