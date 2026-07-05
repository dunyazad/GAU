#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include "model/GraphTypes.h"
#include "model/NodeClass.h"
#include "model/UserType.h"

#include <string>
#include <vector>

struct ClassEditorAction
{
    enum class Type
    {
        None,
        // Validated class submission; the fields below carry the new
        // class definition.
        Submit,
        // Validated user-type submission; the type* fields carry it.
        SubmitType,
        // Delete the class/type being edited. name carries the target name;
        // editTarget is set for a node class, null for a user type.
        Delete,
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

    // SubmitType payload.
    UserType userType;
    // Previous name when a type edit renames it (empty for a new type).
    std::string typeEditOldName;
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
    std::string typeName;
    std::string keyTypeName;
};

// One selectable entry of a type dropdown: a builtin PinType or a user
// type (type == PinType::UserType, typeName set). label is the display
// text (builtin keyword or the user type name).
struct TypeOption
{
    PinType type = PinType::Float;
    std::string typeName;
    std::string label;
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
    TypeKind,
    FieldType,
    LoadType,
};

// Whether the dialog is authoring a node class or a user-defined type.
enum class DialogEditMode
{
    Class,
    Type,
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
    static constexpr float TAB_HEIGHT = 24.0f * UI_SCALE;
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
        TypeName,
        EnumValue,
        StructFieldName,
    };

    bool IsOpen() const { return open; }

    // Opens centered, with one default exec input pin and no properties.
    void Open(float screenWidth, float screenHeight);
    // Opens prefilled with an existing (dynamic) class for editing.
    void OpenForEdit(const NodeClass& target, float screenWidth, float screenHeight);
    void Close();

    bool IsEditMode() const { return editTarget != nullptr; }
    DialogEditMode GetMode() const { return mode; }
    const std::string& GetTypeNameText() const { return typeNameText; }
    UserTypeKind GetTypeKind() const { return typeKind; }
    const std::vector<std::string>& GetEnumValues() const { return enumValues; }
    const std::vector<StructField>& GetStructFields() const { return structFields; }
    int GetFocusedEnumIndex() const { return focusedEnumIndex; }

    // Handles one input event while open; the dialog consumes all events.
    ClassEditorAction HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    // True when an existing class/type is open for editing, so a Delete
    // button is offered.
    bool CanDelete() const;
    const std::string& GetClassNameText() const { return classNameText; }
    const std::string& GetCategoryText() const { return categoryText; }
    // Suggestions shown by the category dropdown (existing categories).
    const std::vector<std::string>& GetCategoryOptions() const { return categoryOptions; }
    const std::vector<PinDef>& GetPins() const { return pins; }
    const std::vector<PropertyDraft>& GetProperties() const { return properties; }
    // Entries of the currently open type dropdown (PinType/PropertyType/
    // PropertyKeyType); empty for other dropdown kinds.
    const std::vector<TypeOption>& GetTypeOptions() const { return typeOptions; }
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

    // Mode tabs (always visible below the title).
    UIRect TabClassRect() const;
    UIRect TabTypeRect() const;

    // Type-mode layout rects.
    UIRect TypeNameFieldRect() const;
    UIRect LoadTypeRect() const;
    UIRect TypeKindRect() const;
    float EnumValuesLabelCenterY() const;
    UIRect EnumValueRect(int valueIndex) const;
    UIRect EnumValueRemoveRect(int valueIndex) const;
    UIRect AddEnumValueButtonRect() const;
    UIRect FieldNameRect(int fieldIndex) const;
    UIRect FieldTypeRect(int fieldIndex) const;

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
    // Delete button (left of OK); only meaningful when CanDelete() is true.
    UIRect DeleteButtonRect() const;
    // Grab strip at the top of the panel: dragging it moves the window.
    UIRect TitleBarRect() const;

private:
    float TabsRowY() const;
    float ContentTopY() const;
    float TypeNameRowY() const;
    float TypeKindRowY() const;
    float EnumValuesLabelY() const;
    float EnumValueRowY(int valueIndex) const;
    float AddEnumValueRowY() const;
    float TypeButtonRowY() const;
    // Member rows shown in Type mode: enum values, struct fields, or none.
    int TypeMemberCount() const;
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
    void HandleTypeModeMouseDown(float x, float y);
    void SwitchMode(DialogEditMode newMode);
    void LoadExistingType(int registryIndex);
    void AppendText(const char* text);
    void HandleBackspace();
    ClassEditorAction TrySubmit();
    ClassEditorAction TrySubmitType();

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    // Title-bar drag state (window move).
    bool draggingTitle = false;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    DialogEditMode mode = DialogEditMode::Class;
    std::string typeNameText;
    UserTypeKind typeKind = UserTypeKind::Enum;
    std::vector<std::string> enumValues;
    std::vector<StructField> structFields;
    int focusedEnumIndex = -1;
    std::string typeEditOldName;
    std::string categoryText;
    std::vector<std::string> categoryOptions;
    std::vector<TypeOption> typeOptions;
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
