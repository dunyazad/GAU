#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include "model/NodeGraph.h"

#include <string>
#include <vector>

struct PropertyPanelAction
{
    enum class Type
    {
        None,
        // Commit an edited value: propertyIndex (+ fieldIndex for a struct
        // field, else -1) and the entered text (the caller parses and
        // pushes the undo command).
        SetProperty,
    };

    Type type = Type::None;
    int propertyIndex = -1;
    int fieldIndex = -1;
    std::string text;
};

// One visible line of the panel. A scalar/container property is a single
// editable row (fieldIndex == -1). A struct property expands to a
// non-editable header row (isHeader) followed by one editable row per
// field (fieldIndex >= 0, indented).
struct PropertyRow
{
    int propertyIndex = -1;
    int fieldIndex = -1;
    bool isHeader = false;
    int depth = 0;
};

// Property inspector for the selected node. Docked to the right edge by
// default; dragging its header tears it off to float, and dropping it
// near the right edge docks it again. Owns state and hit testing only;
// drawing lives in render/PropertyPanelRenderer.
class PropertyPanel
{
public:
    static constexpr float WIDTH = 280.0f * UI_SCALE;
    static constexpr float HEADER_HEIGHT = 28.0f * UI_SCALE;
    static constexpr float ROW_STRIDE = 30.0f * UI_SCALE;
    static constexpr float ROW_HEIGHT = 26.0f * UI_SCALE;
    static constexpr float PADDING = 8.0f * UI_SCALE;
    static constexpr float LABEL_WIDTH = 110.0f * UI_SCALE;
    // Dropping the panel this close to the right edge re-docks it.
    static constexpr float DOCK_SNAP = 48.0f * UI_SCALE;
    // Panel top offset (below the tab bar).
    static constexpr float TOP_OFFSET = 30.0f * UI_SCALE;

    // Updates the shown node each frame; a target change drops focus.
    void SetTarget(const Node* node);

    bool HasTarget() const { return targetNodeId != INVALID_ID; }
    bool IsEditingField() const { return focusedProperty >= 0; }

    // The panel's visible rows for a node (struct properties expanded).
    std::vector<PropertyRow> BuildRows(const Node* node) const;
    int GetFocusedField() const { return focusedField; }

    // Handles one input event. Returns true when consumed by the panel;
    // outAction may carry a commit even when not consumed (e.g. a click
    // outside the panel commits the focused field first).
    bool HandleEvent(const EditorInputEvent& event, const Node* node,
                     float screenWidth, float screenHeight, PropertyPanelAction& outAction);

    float GetX(float screenWidth) const;
    float GetY() const;
    float GetPanelHeight(const Node* node) const;
    bool IsDocked() const { return docked; }
    int GetFocusedProperty() const { return focusedProperty; }
    const std::string& GetEditText() const { return editText; }

    UIRect PanelRect(const Node* node, float screenWidth) const;
    UIRect HeaderRect(float screenWidth) const;
    // Editable field box for the row at rowIndex (indented by depth).
    UIRect FieldRect(int rowIndex, float screenWidth) const;
    UIRect FieldRectForDepth(int rowIndex, int depth, float screenWidth) const;

private:
    void CommitFocus(PropertyPanelAction& outAction);
    // Handles a click on an editable row: fills outAction for a toggle/
    // cycle, or returns true to start text editing (editText set).
    bool EditRowOnClick(const Node& node, const std::vector<PropertyDef>& defs,
                        const PropertyRow& row, PropertyPanelAction& outAction);

    NodeId targetNodeId = INVALID_ID;
    bool docked = true;
    float floatX = 0.0f;
    float floatY = 0.0f;
    bool draggingHeader = false;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    int focusedProperty = -1;
    int focusedField = -1;
    std::string editText;
};
