#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include "model/GraphTypes.h"
#include "model/NodeClass.h"

#include <string>
#include <vector>

struct ClassEditorAction
{
    enum class Type
    {
        None,
        // Validated submission; the dialog has closed itself and the
        // fields below carry the new class definition.
        Submit,
        // Dialog cancelled/closed without creating anything.
        Closed,
    };

    Type type = Type::None;
    std::string name;
    std::string category;
    std::vector<PinDef> pins;
    std::vector<PropertyDef> properties;
    // Non-null when this submission edits an existing class.
    const NodeClass* editTarget = nullptr;
};

// Editable property row: the default value is kept as raw text while
// typing and converted to typed values on submit. Container defaults:
// Array/Set = comma-separated ("1, 2, 3"), Map = "key:value, key:value".
struct PropertyDraft
{
    PropertyContainer container = PropertyContainer::None;
    PinType type = PinType::Float;
    PinType keyType = PinType::String;
    std::string name;
    std::string defaultText;
};

// Which dropdown is open, if any. Row-scoped kinds use a row index.
enum class DialogDropdownKind
{
    None,
    Category,
    PinType,
    PropertyContainer,
    PropertyType,
    PropertyKeyType,
};

// Modal dialog for registering a custom NodeClass at runtime: class name
// text field, dropdowns for category and types, editable pin rows,
// editable property rows (scalar or array/set/map with defaults) and
// Create/Cancel. Owns state and hit testing only; drawing lives in
// render/ClassEditorDialogRenderer, which shares the rect getters below.
class ClassEditorDialog
{
public:
    static constexpr float WIDTH = 420.0f * UI_SCALE;
    static constexpr float PADDING = 12.0f * UI_SCALE;
    static constexpr float TITLE_HEIGHT = 22.0f * UI_SCALE;
    static constexpr float ROW_HEIGHT = 26.0f * UI_SCALE;
    static constexpr float GAP = 8.0f * UI_SCALE;
    static constexpr float LABEL_WIDTH = 80.0f * UI_SCALE;
    static constexpr float SECTION_LABEL_HEIGHT = 20.0f * UI_SCALE;
    static constexpr float PIN_ROW_STRIDE = 30.0f * UI_SCALE;
    static constexpr float ERROR_HEIGHT = 18.0f * UI_SCALE;
    static constexpr float BUTTON_WIDTH = 96.0f * UI_SCALE;
    static constexpr float BUTTON_HEIGHT = 28.0f * UI_SCALE;

    enum class Focus
    {
        None,
        ClassName,
        Category,
        PinName,
        PropertyName,
        PropertyDefault,
    };

    bool IsOpen() const { return open; }

    // Opens centered, with one default exec input pin and no properties.
    void Open(float screenWidth, float screenHeight);
    // Opens prefilled with an existing (dynamic) class for editing.
    void OpenForEdit(const NodeClass& target, float screenWidth, float screenHeight);
    void Close();

    bool IsEditMode() const { return editTarget != nullptr; }

    // Handles one input event while open; the dialog consumes all events.
    ClassEditorAction HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::string& GetClassNameText() const { return classNameText; }
    const std::string& GetCategoryText() const { return categoryText; }
    // Suggestions shown by the category dropdown (existing categories).
    const std::vector<std::string>& GetCategoryOptions() const { return categoryOptions; }
    const std::vector<PinDef>& GetPins() const { return pins; }
    const std::vector<PropertyDraft>& GetProperties() const { return properties; }
    Focus GetFocus() const { return focus; }
    int GetFocusedPinIndex() const { return focusedPinIndex; }
    int GetFocusedPropertyIndex() const { return focusedPropertyIndex; }
    const std::string& GetErrorText() const { return errorText; }

    // Open dropdown state, shared with the renderer.
    DialogDropdownKind GetDropdownKind() const { return dropdownKind; }
    int GetDropdownRowIndex() const { return dropdownRowIndex; }
    int GetDropdownOptionCount() const;
    int GetDropdownSelectedIndex() const;
    int GetDropdownHoverIndex() const { return dropdownHoverIndex; }
    UIRect DropdownAnchorRect() const;
    UIRect DropdownListRect() const;
    UIRect DropdownOptionRect(int optionIndex) const;

    // Layout rects shared by hit testing and the renderer.
    UIRect NameFieldRect() const;
    // Editable category text field plus the suggestion dropdown arrow.
    UIRect CategoryFieldRect() const;
    UIRect CategoryDropdownRect() const;
    float PinsLabelCenterY() const;
    UIRect PinDirectionRect(int pinIndex) const;
    UIRect PinTypeRect(int pinIndex) const;
    UIRect PinNameRect(int pinIndex) const;
    UIRect PinRemoveRect(int pinIndex) const;
    UIRect AddPinButtonRect() const;
    float PropertiesLabelCenterY() const;
    UIRect PropertyContainerRect(int propertyIndex) const;
    UIRect PropertyTypeRect(int propertyIndex) const;
    UIRect PropertyKeyTypeRect(int propertyIndex) const;
    UIRect PropertyNameRect(int propertyIndex) const;
    UIRect PropertyDefaultRect(int propertyIndex) const;
    UIRect PropertyRemoveRect(int propertyIndex) const;
    UIRect AddPropertyButtonRect() const;
    float ErrorCenterY() const;
    UIRect OkButtonRect() const;
    UIRect CancelButtonRect() const;

private:
    float NameRowY() const;
    float CategoryRowY() const;
    float PinsLabelY() const;
    float PinRowY(int pinIndex) const;
    float AddPinRowY() const;
    float PropertiesLabelY() const;
    float PropertyRowY(int propertyIndex) const;
    float AddPropertyRowY() const;
    float ErrorRowY() const;
    float ButtonRowY() const;

    void OpenDropdown(DialogDropdownKind kind, int rowIndex);
    void CloseDropdown();
    void ApplyDropdownSelection(int optionIndex);
    bool HandleDropdownMouseDown(float x, float y);
    void HandleMouseDown(float x, float y);
    void AppendText(const char* text);
    void HandleBackspace();
    ClassEditorAction TrySubmit();

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    std::string categoryText;
    std::vector<std::string> categoryOptions;
    std::string classNameText;
    std::vector<PinDef> pins;
    std::vector<PropertyDraft> properties;
    Focus focus = Focus::None;
    int focusedPinIndex = -1;
    int focusedPropertyIndex = -1;
    std::string errorText;
    const NodeClass* editTarget = nullptr;
    DialogDropdownKind dropdownKind = DialogDropdownKind::None;
    int dropdownRowIndex = -1;
    int dropdownHoverIndex = -1;
};
