#include "ClassEditorDialog.h"

#include "model/UserType.h"

#include <cctype>

static const std::size_t MAX_CLASS_NAME_LENGTH = 48;
static const std::size_t MAX_CATEGORY_LENGTH = 32;
static const std::size_t MAX_PIN_NAME_LENGTH = 32;
static const std::size_t MAX_DEFAULT_TEXT_LENGTH = 96;

static std::string TrimAscii(const std::string& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return text.substr(begin, end - begin);
}

static std::vector<std::string> SplitTrimmed(const std::string& text, char separator)
{
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == separator) {
            parts.push_back(TrimAscii(current));
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(TrimAscii(current));
    return parts;
}

// "Name" -> "Name2" -> "Name3" ... until isTaken returns false.
template <typename IsTakenFn>
static std::string MakeUniqueName(const std::string& baseName, IsTakenFn isTaken)
{
    if (baseName.empty()) {
        return baseName;
    }
    std::string candidate = baseName;
    int suffix = 2;
    while (isTaken(candidate)) {
        candidate = baseName + std::to_string(suffix);
        ++suffix;
    }
    return candidate;
}

static std::string MakeUniqueDraftName(const std::string& baseName,
                                       const std::vector<PropertyDraft>& drafts)
{
    return MakeUniqueName(baseName, [&drafts](const std::string& candidate) {
        for (const PropertyDraft& draft : drafts) {
            if (draft.name == candidate) {
                return true;
            }
        }
        return false;
    });
}

static std::string MakeUniquePinName(const std::string& baseName,
                                     const std::vector<PinDef>& pins)
{
    return MakeUniqueName(baseName, [&pins](const std::string& candidate) {
        for (const PinDef& pin : pins) {
            if (pin.name == candidate) {
                return true;
            }
        }
        return false;
    });
}

static void PopLastUTF8Character(std::string& text)
{
    while (!text.empty() && (static_cast<unsigned char>(text.back()) & 0xC0) == 0x80) {
        text.pop_back();
    }
    if (!text.empty()) {
        text.pop_back();
    }
}

void ClassEditorDialog::Open(float screenWidth, float screenHeight)
{
    open = true;
    editTarget = nullptr;
    mode = DialogEditMode::Class;
    typeNameText.clear();
    typeKind = UserTypeKind::Enum;
    enumValues.clear();
    structFields.clear();
    focusedEnumIndex = -1;
    typeEditOldName.clear();
    categoryText = "Function";
    classNameText.clear();
    pins.clear();
    keptExecOutputs.clear();
    properties.clear();

    // Exec pins are added automatically from the category on save
    // (ExecPinPolicy); the dialog edits data pins only.
    PinDef defaultPin;
    defaultPin.direction = PinDirection::Input;
    defaultPin.type = PinType::Float;
    pins.push_back(defaultPin);

    focus = Focus::ClassName;
    focusedPinIndex = -1;
    focusedPropertyIndex = -1;
    errorText.clear();
    CloseDropdown();

    panelX = (screenWidth - WIDTH) * 0.5f;
    panelY = (screenHeight - GetPanelHeight()) * 0.5f;
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

// Renders a PropertyDef's default back into the dialog's editable text
// form (inverse of ConvertPropertyDraft).
static std::string BuildDefaultText(const PropertyDef& def)
{
    switch (def.container) {
    case PropertyContainer::None:
        return ValueToString(def.defaultValue);

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        std::string joined;
        for (const Value& element : def.defaultElements) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(element);
        }
        return joined;
    }

    case PropertyContainer::Map: {
        std::string joined;
        for (const std::pair<Value, Value>& entry : def.defaultEntries) {
            if (!joined.empty()) {
                joined += ", ";
            }
            joined += ValueToString(entry.first) + ":" + ValueToString(entry.second);
        }
        return joined;
    }
    }
    return std::string();
}

void ClassEditorDialog::OpenForEdit(const NodeClass& target, float screenWidth, float screenHeight)
{
    Open(screenWidth, screenHeight);
    editTarget = &target;
    classNameText = target.GetName();
    categoryText = target.GetCategory();
    // The dialog edits data pins only; exec outputs the class already
    // has ride along invisibly so saving keeps them.
    pins.clear();
    keptExecOutputs.clear();
    for (const PinDef& pin : target.GetPins()) {
        if (pin.type != PinType::Exec) {
            pins.push_back(pin);
        } else if (pin.direction == PinDirection::Output) {
            keptExecOutputs.push_back(pin);
        }
    }

    properties.clear();
    for (const PropertyDef& def : target.GetProperties()) {
        PropertyDraft draft;
        draft.container = def.container;
        draft.type = def.type;
        draft.typeName = def.typeName;
        draft.keyType = def.keyType;
        draft.keyTypeName = def.keyTypeName;
        draft.name = def.name;
        draft.defaultText = BuildDefaultText(def);
        properties.push_back(std::move(draft));
    }

    // Recenter for the prefilled height.
    panelY = (screenHeight - GetPanelHeight()) * 0.5f;
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

void ClassEditorDialog::Close()
{
    open = false;
    draggingTitle = false;
    editTarget = nullptr;
    mode = DialogEditMode::Class;
    typeNameText.clear();
    enumValues.clear();
    structFields.clear();
    focusedEnumIndex = -1;
    typeEditOldName.clear();
    classNameText.clear();
    pins.clear();
    properties.clear();
    errorText.clear();
    focus = Focus::None;
    focusedPinIndex = -1;
    focusedPropertyIndex = -1;
    CloseDropdown();
}

float ClassEditorDialog::GetPanelHeight() const
{
    const float header = PADDING + TITLE_HEIGHT + GAP + TAB_HEIGHT + GAP;
    if (mode == DialogEditMode::Type) {
        return header
             + ROW_HEIGHT + GAP
             + ROW_HEIGHT + GAP
             + SECTION_LABEL_HEIGHT
             + PIN_ROW_STRIDE * static_cast<float>(TypeMemberCount())
             + GAP + BUTTON_HEIGHT
             + GAP + ERROR_HEIGHT
             + GAP + BUTTON_HEIGHT
             + PADDING;
    }
    return header
         + ROW_HEIGHT + GAP
         + ROW_HEIGHT + GAP
         + SECTION_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(pins.size())
         + GAP + BUTTON_HEIGHT
         + GAP + SECTION_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(properties.size())
         + GAP + BUTTON_HEIGHT
         + GAP + ERROR_HEIGHT
         + GAP + BUTTON_HEIGHT
         + PADDING;
}

float ClassEditorDialog::TabsRowY() const
{
    return panelY + PADDING + TITLE_HEIGHT + GAP;
}

float ClassEditorDialog::ContentTopY() const
{
    return TabsRowY() + TAB_HEIGHT + GAP;
}

float ClassEditorDialog::TypeNameRowY() const
{
    return ContentTopY();
}

float ClassEditorDialog::TypeKindRowY() const
{
    return TypeNameRowY() + ROW_HEIGHT + GAP;
}

float ClassEditorDialog::EnumValuesLabelY() const
{
    return TypeKindRowY() + ROW_HEIGHT + GAP;
}

float ClassEditorDialog::EnumValueRowY(int valueIndex) const
{
    return EnumValuesLabelY() + SECTION_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(valueIndex);
}

int ClassEditorDialog::TypeMemberCount() const
{
    if (typeKind == UserTypeKind::Enum) {
        return static_cast<int>(enumValues.size());
    }
    if (typeKind == UserTypeKind::Struct) {
        return static_cast<int>(structFields.size());
    }
    return 0;
}

float ClassEditorDialog::AddEnumValueRowY() const
{
    return EnumValueRowY(TypeMemberCount()) + GAP;
}

float ClassEditorDialog::TypeButtonRowY() const
{
    return AddEnumValueRowY() + BUTTON_HEIGHT + GAP + ERROR_HEIGHT + GAP;
}

float ClassEditorDialog::NameRowY() const
{
    return ContentTopY();
}

float ClassEditorDialog::CategoryRowY() const
{
    return NameRowY() + ROW_HEIGHT + GAP;
}

float ClassEditorDialog::PinsLabelY() const
{
    return CategoryRowY() + ROW_HEIGHT + GAP;
}

float ClassEditorDialog::PinsLabelCenterY() const
{
    return PinsLabelY() + SECTION_LABEL_HEIGHT * 0.5f;
}

float ClassEditorDialog::PinRowY(int pinIndex) const
{
    return PinsLabelY() + SECTION_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(pinIndex);
}

float ClassEditorDialog::AddPinRowY() const
{
    return PinRowY(static_cast<int>(pins.size())) + GAP;
}

float ClassEditorDialog::PropertiesLabelY() const
{
    return AddPinRowY() + BUTTON_HEIGHT + GAP;
}

float ClassEditorDialog::PropertiesLabelCenterY() const
{
    return PropertiesLabelY() + SECTION_LABEL_HEIGHT * 0.5f;
}

float ClassEditorDialog::PropertyRowY(int propertyIndex) const
{
    return PropertiesLabelY() + SECTION_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(propertyIndex);
}

float ClassEditorDialog::AddPropertyRowY() const
{
    return PropertyRowY(static_cast<int>(properties.size())) + GAP;
}

float ClassEditorDialog::ErrorRowY() const
{
    if (mode == DialogEditMode::Type) {
        return AddEnumValueRowY() + BUTTON_HEIGHT + GAP;
    }
    return AddPropertyRowY() + BUTTON_HEIGHT + GAP;
}

float ClassEditorDialog::ErrorCenterY() const
{
    return ErrorRowY() + ERROR_HEIGHT * 0.5f;
}

float ClassEditorDialog::ButtonRowY() const
{
    return ErrorRowY() + ERROR_HEIGHT + GAP;
}

UIRect ClassEditorDialog::TabClassRect() const
{
    const float tabWidth = (WIDTH - PADDING * 2.0f - GAP) * 0.5f;
    return UIRect{panelX + PADDING, TabsRowY(), tabWidth, TAB_HEIGHT};
}

UIRect ClassEditorDialog::TabTypeRect() const
{
    const float tabWidth = (WIDTH - PADDING * 2.0f - GAP) * 0.5f;
    return UIRect{panelX + PADDING + tabWidth + GAP, TabsRowY(), tabWidth, TAB_HEIGHT};
}

UIRect ClassEditorDialog::TypeNameFieldRect() const
{
    const float loadWidth = 72.0f * UI_SCALE;
    return UIRect{panelX + PADDING + LABEL_WIDTH, TypeNameRowY(),
                  WIDTH - PADDING * 2.0f - LABEL_WIDTH - loadWidth - GAP, ROW_HEIGHT};
}

UIRect ClassEditorDialog::LoadTypeRect() const
{
    const float loadWidth = 72.0f * UI_SCALE;
    return UIRect{panelX + WIDTH - PADDING - loadWidth, TypeNameRowY(), loadWidth, ROW_HEIGHT};
}

UIRect ClassEditorDialog::TypeKindRect() const
{
    return UIRect{panelX + PADDING + LABEL_WIDTH, TypeKindRowY(), 120.0f * UI_SCALE, ROW_HEIGHT};
}

float ClassEditorDialog::EnumValuesLabelCenterY() const
{
    return EnumValuesLabelY() + SECTION_LABEL_HEIGHT * 0.5f;
}

UIRect ClassEditorDialog::EnumValueRect(int valueIndex) const
{
    const float left = panelX + PADDING;
    const float right = panelX + WIDTH - PADDING - 28.0f * UI_SCALE;
    return UIRect{left, EnumValueRowY(valueIndex), right - left, ROW_HEIGHT};
}

UIRect ClassEditorDialog::EnumValueRemoveRect(int valueIndex) const
{
    return UIRect{panelX + WIDTH - PADDING - 22.0f * UI_SCALE, EnumValueRowY(valueIndex),
                  22.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::AddEnumValueButtonRect() const
{
    return UIRect{panelX + PADDING, AddEnumValueRowY(), 110.0f * UI_SCALE, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::FieldNameRect(int fieldIndex) const
{
    return UIRect{panelX + PADDING, EnumValueRowY(fieldIndex), 110.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::FieldTypeRect(int fieldIndex) const
{
    return UIRect{panelX + PADDING + 118.0f * UI_SCALE, EnumValueRowY(fieldIndex),
                  90.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::NameFieldRect() const
{
    return UIRect{panelX + PADDING + LABEL_WIDTH, NameRowY(),
                  WIDTH - PADDING * 2.0f - LABEL_WIDTH, ROW_HEIGHT};
}

UIRect ClassEditorDialog::CategoryFieldRect() const
{
    return UIRect{panelX + PADDING + LABEL_WIDTH, CategoryRowY(), 140.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::CategoryDropdownRect() const
{
    const UIRect field = CategoryFieldRect();
    return UIRect{field.x + field.w, field.y, 26.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinDirectionRect(int pinIndex) const
{
    return UIRect{panelX + PADDING, PinRowY(pinIndex), 46.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinTypeRect(int pinIndex) const
{
    return UIRect{panelX + PADDING + 52.0f * UI_SCALE, PinRowY(pinIndex), 64.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinNameRect(int pinIndex) const
{
    const float left = panelX + PADDING + 122.0f * UI_SCALE;
    const float right = panelX + WIDTH - PADDING - 28.0f * UI_SCALE;
    return UIRect{left, PinRowY(pinIndex), right - left, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinRemoveRect(int pinIndex) const
{
    return UIRect{panelX + WIDTH - PADDING - 22.0f * UI_SCALE, PinRowY(pinIndex),
                  22.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::AddPinButtonRect() const
{
    return UIRect{panelX + PADDING, AddPinRowY(), 100.0f * UI_SCALE, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::PropertyContainerRect(int propertyIndex) const
{
    return UIRect{panelX + PADDING, PropertyRowY(propertyIndex), 52.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PropertyTypeRect(int propertyIndex) const
{
    return UIRect{panelX + PADDING + 56.0f * UI_SCALE, PropertyRowY(propertyIndex),
                  52.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PropertyKeyTypeRect(int propertyIndex) const
{
    return UIRect{panelX + PADDING + 112.0f * UI_SCALE, PropertyRowY(propertyIndex),
                  52.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PropertyNameRect(int propertyIndex) const
{
    return UIRect{panelX + PADDING + 168.0f * UI_SCALE, PropertyRowY(propertyIndex),
                  100.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PropertyDefaultRect(int propertyIndex) const
{
    const float left = panelX + PADDING + 272.0f * UI_SCALE;
    const float right = panelX + WIDTH - PADDING - 28.0f * UI_SCALE;
    return UIRect{left, PropertyRowY(propertyIndex), right - left, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PropertyRemoveRect(int propertyIndex) const
{
    return UIRect{panelX + WIDTH - PADDING - 22.0f * UI_SCALE, PropertyRowY(propertyIndex),
                  22.0f * UI_SCALE, ROW_HEIGHT};
}

UIRect ClassEditorDialog::AddPropertyButtonRect() const
{
    return UIRect{panelX + PADDING, AddPropertyRowY(), 130.0f * UI_SCALE, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::OkButtonRect() const
{
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH * 2.0f - GAP, ButtonRowY(),
                  BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::CancelButtonRect() const
{
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH, ButtonRowY(),
                  BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::DeleteButtonRect() const
{
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH * 3.0f - GAP * 2.0f, ButtonRowY(),
                  BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect ClassEditorDialog::TitleBarRect() const
{
    return UIRect{panelX, panelY, WIDTH, PADDING + TITLE_HEIGHT};
}

bool ClassEditorDialog::CanDelete() const
{
    if (mode == DialogEditMode::Type) {
        return !typeEditOldName.empty();
    }
    return editTarget != nullptr;
}

int ClassEditorDialog::GetDropdownOptionCount() const
{
    switch (dropdownKind) {
    case DialogDropdownKind::None:
        return 0;
    case DialogDropdownKind::Category:
        return static_cast<int>(categoryOptions.size());
    case DialogDropdownKind::PinType:
        return static_cast<int>(typeOptions.size());
    case DialogDropdownKind::PropertyContainer:
        return PROPERTY_CONTAINER_COUNT;
    case DialogDropdownKind::PropertyType:
    case DialogDropdownKind::PropertyKeyType:
        return static_cast<int>(typeOptions.size());
    case DialogDropdownKind::TypeKind:
        return 3;
    case DialogDropdownKind::FieldType:
        return static_cast<int>(typeOptions.size());
    case DialogDropdownKind::LoadType:
        return static_cast<int>(UserTypeRegistry::GetAll().size());
    }
    return 0;
}

// User type kinds in dropdown order.
static const UserTypeKind TYPE_KIND_ORDER[3] = {
    UserTypeKind::Enum,
    UserTypeKind::Struct,
    UserTypeKind::ObjectAlias,
};

// Index of the option in typeOptions matching a (type, typeName) pair.
static int FindTypeOption(const std::vector<TypeOption>& options, PinType type,
                          const std::string& typeName)
{
    for (int i = 0; i < static_cast<int>(options.size()); ++i) {
        if (options[static_cast<std::size_t>(i)].type != type) {
            continue;
        }
        if (type != PinType::UserType
            || options[static_cast<std::size_t>(i)].typeName == typeName) {
            return i;
        }
    }
    return -1;
}

int ClassEditorDialog::GetDropdownSelectedIndex() const
{
    switch (dropdownKind) {
    case DialogDropdownKind::None:
        return -1;
    case DialogDropdownKind::Category:
        for (int i = 0; i < static_cast<int>(categoryOptions.size()); ++i) {
            if (categoryOptions[static_cast<std::size_t>(i)] == categoryText) {
                return i;
            }
        }
        return -1;
    case DialogDropdownKind::PinType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(pins.size())) {
            const PinDef& pin = pins[static_cast<std::size_t>(dropdownRowIndex)];
            return FindTypeOption(typeOptions, pin.type, pin.typeName);
        }
        return -1;
    case DialogDropdownKind::PropertyContainer:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            return PropertyContainerIndex(
                properties[static_cast<std::size_t>(dropdownRowIndex)].container);
        }
        return -1;
    case DialogDropdownKind::PropertyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            const PropertyDraft& property = properties[static_cast<std::size_t>(dropdownRowIndex)];
            return FindTypeOption(typeOptions, property.type, property.typeName);
        }
        return -1;
    case DialogDropdownKind::PropertyKeyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            const PropertyDraft& property = properties[static_cast<std::size_t>(dropdownRowIndex)];
            return FindTypeOption(typeOptions, property.keyType, property.keyTypeName);
        }
        return -1;
    case DialogDropdownKind::TypeKind:
        for (int i = 0; i < 3; ++i) {
            if (TYPE_KIND_ORDER[i] == typeKind) {
                return i;
            }
        }
        return -1;
    case DialogDropdownKind::FieldType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(structFields.size())) {
            const StructField& field = structFields[static_cast<std::size_t>(dropdownRowIndex)];
            return FindTypeOption(typeOptions, field.type, field.typeName);
        }
        return -1;
    case DialogDropdownKind::LoadType:
        return -1;
    }
    return -1;
}

UIRect ClassEditorDialog::DropdownAnchorRect() const
{
    switch (dropdownKind) {
    case DialogDropdownKind::None:
        break;
    case DialogDropdownKind::Category: {
        const UIRect field = CategoryFieldRect();
        const UIRect arrow = CategoryDropdownRect();
        return UIRect{field.x, field.y, field.w + arrow.w, field.h};
    }
    case DialogDropdownKind::PinType:
        return PinTypeRect(dropdownRowIndex);
    case DialogDropdownKind::PropertyContainer:
        return PropertyContainerRect(dropdownRowIndex);
    case DialogDropdownKind::PropertyType:
        return PropertyTypeRect(dropdownRowIndex);
    case DialogDropdownKind::PropertyKeyType:
        return PropertyKeyTypeRect(dropdownRowIndex);
    case DialogDropdownKind::TypeKind:
        return TypeKindRect();
    case DialogDropdownKind::FieldType:
        return FieldTypeRect(dropdownRowIndex);
    case DialogDropdownKind::LoadType:
        return LoadTypeRect();
    }
    return UIRect();
}

UIRect ClassEditorDialog::DropdownListRect() const
{
    const UIRect anchor = DropdownAnchorRect();
    const float width = (anchor.w > 120.0f * UI_SCALE) ? anchor.w : 120.0f * UI_SCALE;
    const float height = ROW_HEIGHT * static_cast<float>(GetDropdownOptionCount());
    return UIRect{anchor.x, anchor.y + anchor.h, width, height};
}

UIRect ClassEditorDialog::DropdownOptionRect(int optionIndex) const
{
    const UIRect list = DropdownListRect();
    return UIRect{list.x, list.y + ROW_HEIGHT * static_cast<float>(optionIndex),
                  list.w, ROW_HEIGHT};
}

// Capitalized display name matching the type dropdown rows.
static const char* BuiltinTypeLabel(PinType type)
{
    switch (type) {
    case PinType::Exec:
        return "Exec";
    case PinType::Bool:
        return "Bool";
    case PinType::Int:
        return "Int";
    case PinType::Float:
        return "Float";
    case PinType::String:
        return "String";
    case PinType::Object:
        return "Object";
    case PinType::UserType:
        return "";
    }
    return "";
}

// Fills typeOptions with the builtin types followed by the matching user
// types. excludeObjectAlias drops object-alias user types (used where a
// value is required: property element/key types accept enums and structs,
// but not opaque object aliases).
static void BuildTypeOptions(std::vector<TypeOption>& out, const PinType* builtins,
                             int builtinCount, bool excludeObjectAlias)
{
    out.clear();
    for (int i = 0; i < builtinCount; ++i) {
        TypeOption option;
        option.type = builtins[i];
        option.label = BuiltinTypeLabel(builtins[i]);
        out.push_back(std::move(option));
    }
    for (const UserType& userType : UserTypeRegistry::GetAll()) {
        if (excludeObjectAlias && userType.kind == UserTypeKind::ObjectAlias) {
            continue;
        }
        TypeOption option;
        option.type = PinType::UserType;
        option.typeName = userType.name;
        option.label = userType.name;
        out.push_back(std::move(option));
    }
}

// Appends node classes categorized as "Object" as selectable object-by-name
// (UserType) options, deduped against options already present. This lets an
// Object-category node class be used directly as a pin type (nominal object
// type matched by name), without first defining a user Type.
static void AppendObjectClassTypes(std::vector<TypeOption>& out)
{
    for (const NodeClass* nodeClass : NodeClass::GetRegistry()) {
        if (nodeClass->GetCategory() != "Object") {
            continue;
        }
        const std::string name = nodeClass->GetName();
        bool duplicate = false;
        for (const TypeOption& existing : out) {
            if (existing.type == PinType::UserType && existing.typeName == name) {
                duplicate = true;
                break;
            }
        }
        if (duplicate) {
            continue;
        }
        TypeOption option;
        option.type = PinType::UserType;
        option.typeName = name;
        option.label = name;
        out.push_back(std::move(option));
    }
}

// Builtin types offered for a struct field (any value type or object, but
// not Exec).
static const PinType FIELD_BUILTIN_TYPES[5] = {
    PinType::Bool,
    PinType::Int,
    PinType::Float,
    PinType::String,
    PinType::Object,
};

void ClassEditorDialog::OpenDropdown(DialogDropdownKind kind, int rowIndex)
{
    dropdownKind = kind;
    dropdownRowIndex = rowIndex;
    dropdownHoverIndex = -1;

    if (kind == DialogDropdownKind::PinType) {
        // Data types only: exec pins are category policy, never picked.
        BuildTypeOptions(typeOptions, FIELD_BUILTIN_TYPES, 5, false);
        AppendObjectClassTypes(typeOptions);
    } else if (kind == DialogDropdownKind::PropertyType
               || kind == DialogDropdownKind::PropertyKeyType) {
        BuildTypeOptions(typeOptions, VALUE_PIN_TYPES, VALUE_PIN_TYPE_COUNT, true);
    } else if (kind == DialogDropdownKind::FieldType) {
        BuildTypeOptions(typeOptions, FIELD_BUILTIN_TYPES, 5, false);
        AppendObjectClassTypes(typeOptions);
    }

    if (kind == DialogDropdownKind::Category) {
        // Suggestions: builtin names first, then user-defined categories
        // already present in the registry.
        categoryOptions.clear();
        for (const char* builtinName : BUILTIN_CATEGORY_NAMES) {
            categoryOptions.push_back(builtinName);
        }
        for (const NodeClass* nodeClass : NodeClass::GetRegistry()) {
            bool known = false;
            for (const std::string& existing : categoryOptions) {
                if (existing == nodeClass->GetCategory()) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                categoryOptions.push_back(nodeClass->GetCategory());
            }
        }
    }
}

void ClassEditorDialog::CloseDropdown()
{
    dropdownKind = DialogDropdownKind::None;
    dropdownRowIndex = -1;
    dropdownHoverIndex = -1;
}

void ClassEditorDialog::ApplyDropdownSelection(int optionIndex)
{
    switch (dropdownKind) {
    case DialogDropdownKind::None:
        break;
    case DialogDropdownKind::Category:
        if (optionIndex >= 0 && optionIndex < static_cast<int>(categoryOptions.size())) {
            categoryText = categoryOptions[static_cast<std::size_t>(optionIndex)];
        }
        break;
    case DialogDropdownKind::PinType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(pins.size())
            && optionIndex >= 0 && optionIndex < static_cast<int>(typeOptions.size())) {
            PinDef& pin = pins[static_cast<std::size_t>(dropdownRowIndex)];
            pin.type = typeOptions[static_cast<std::size_t>(optionIndex)].type;
            pin.typeName = typeOptions[static_cast<std::size_t>(optionIndex)].typeName;
        }
        break;
    case DialogDropdownKind::PropertyContainer:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            properties[static_cast<std::size_t>(dropdownRowIndex)].container =
                ALL_PROPERTY_CONTAINERS[optionIndex];
        }
        break;
    case DialogDropdownKind::PropertyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())
            && optionIndex >= 0 && optionIndex < static_cast<int>(typeOptions.size())) {
            PropertyDraft& property = properties[static_cast<std::size_t>(dropdownRowIndex)];
            property.type = typeOptions[static_cast<std::size_t>(optionIndex)].type;
            property.typeName = typeOptions[static_cast<std::size_t>(optionIndex)].typeName;
        }
        break;
    case DialogDropdownKind::PropertyKeyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())
            && optionIndex >= 0 && optionIndex < static_cast<int>(typeOptions.size())) {
            PropertyDraft& property = properties[static_cast<std::size_t>(dropdownRowIndex)];
            property.keyType = typeOptions[static_cast<std::size_t>(optionIndex)].type;
            property.keyTypeName = typeOptions[static_cast<std::size_t>(optionIndex)].typeName;
        }
        break;
    case DialogDropdownKind::TypeKind:
        if (optionIndex >= 0 && optionIndex < 3) {
            typeKind = TYPE_KIND_ORDER[optionIndex];
        }
        break;
    case DialogDropdownKind::FieldType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(structFields.size())
            && optionIndex >= 0 && optionIndex < static_cast<int>(typeOptions.size())) {
            StructField& field = structFields[static_cast<std::size_t>(dropdownRowIndex)];
            field.type = typeOptions[static_cast<std::size_t>(optionIndex)].type;
            field.typeName = typeOptions[static_cast<std::size_t>(optionIndex)].typeName;
        }
        break;
    case DialogDropdownKind::LoadType:
        LoadExistingType(optionIndex);
        break;
    }
}

// Returns true when the click was consumed by the open dropdown.
bool ClassEditorDialog::HandleDropdownMouseDown(float x, float y)
{
    if (dropdownKind == DialogDropdownKind::None) {
        return false;
    }
    const UIRect list = DropdownListRect();
    if (list.Contains(x, y)) {
        const int optionIndex = static_cast<int>((y - list.y) / ROW_HEIGHT);
        if (optionIndex >= 0 && optionIndex < GetDropdownOptionCount()) {
            ApplyDropdownSelection(optionIndex);
        }
    }
    CloseDropdown();
    return true;
}

ClassEditorAction ClassEditorDialog::HandleEvent(const EditorInputEvent& event)
{
    ClassEditorAction action;
    if (!open) {
        return action;
    }

    switch (event.type) {
    case EditorInputType::MouseMove:
        if (draggingTitle) {
            panelX = event.x - dragOffsetX;
            panelY = event.y - dragOffsetY;
            if (panelX < 0.0f) {
                panelX = 0.0f;
            }
            if (panelY < 0.0f) {
                panelY = 0.0f;
            }
            break;
        }
        if (dropdownKind != DialogDropdownKind::None) {
            const UIRect list = DropdownListRect();
            if (list.Contains(event.x, event.y)) {
                dropdownHoverIndex = static_cast<int>((event.y - list.y) / ROW_HEIGHT);
            } else {
                dropdownHoverIndex = -1;
            }
        }
        break;

    case EditorInputType::MouseDown:
        if (event.button == EditorMouseButton::Left) {
            if (HandleDropdownMouseDown(event.x, event.y)) {
                break;
            }
            if (TitleBarRect().Contains(event.x, event.y)) {
                draggingTitle = true;
                dragOffsetX = event.x - panelX;
                dragOffsetY = event.y - panelY;
                break;
            }
            if (TabClassRect().Contains(event.x, event.y)) {
                SwitchMode(DialogEditMode::Class);
                break;
            }
            if (TabTypeRect().Contains(event.x, event.y)) {
                SwitchMode(DialogEditMode::Type);
                break;
            }
            if (CanDelete() && DeleteButtonRect().Contains(event.x, event.y)) {
                action.type = ClassEditorAction::Type::Delete;
                action.name = (mode == DialogEditMode::Type)
                                  ? typeEditOldName
                                  : std::string(editTarget->GetName());
                action.editTarget = (mode == DialogEditMode::Type) ? nullptr : editTarget;
                Close();
                return action;
            }
            if (OkButtonRect().Contains(event.x, event.y)) {
                return mode == DialogEditMode::Type ? TrySubmitType() : TrySubmit();
            }
            if (CancelButtonRect().Contains(event.x, event.y)) {
                Close();
                action.type = ClassEditorAction::Type::Closed;
                return action;
            }
            if (mode == DialogEditMode::Type) {
                HandleTypeModeMouseDown(event.x, event.y);
            } else {
                HandleMouseDown(event.x, event.y);
            }
        }
        break;

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Escape) {
            if (dropdownKind != DialogDropdownKind::None) {
                CloseDropdown();
            } else {
                Close();
                action.type = ClassEditorAction::Type::Closed;
            }
        } else if (event.key == EditorKey::Enter) {
            if (dropdownKind == DialogDropdownKind::None) {
                return mode == DialogEditMode::Type ? TrySubmitType() : TrySubmit();
            }
        } else if (event.key == EditorKey::Backspace) {
            HandleBackspace();
        }
        break;

    case EditorInputType::TextInput:
        AppendText(event.text);
        break;

    case EditorInputType::MouseUp:
        draggingTitle = false;
        break;

    case EditorInputType::MouseWheel:
        break;
    }

    return action;
}

void ClassEditorDialog::HandleMouseDown(float x, float y)
{
    if (NameFieldRect().Contains(x, y)) {
        focus = Focus::ClassName;
        focusedPinIndex = -1;
        focusedPropertyIndex = -1;
        return;
    }
    if (CategoryFieldRect().Contains(x, y)) {
        focus = Focus::Category;
        focusedPinIndex = -1;
        focusedPropertyIndex = -1;
        return;
    }
    if (CategoryDropdownRect().Contains(x, y)) {
        OpenDropdown(DialogDropdownKind::Category, -1);
        return;
    }
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        PinDef& pin = pins[static_cast<std::size_t>(i)];
        if (PinDirectionRect(i).Contains(x, y)) {
            pin.direction = (pin.direction == PinDirection::Input)
                                ? PinDirection::Output
                                : PinDirection::Input;
            return;
        }
        if (PinTypeRect(i).Contains(x, y)) {
            OpenDropdown(DialogDropdownKind::PinType, i);
            return;
        }
        if (PinNameRect(i).Contains(x, y)) {
            focus = Focus::PinName;
            focusedPinIndex = i;
            focusedPropertyIndex = -1;
            return;
        }
        if (PinRemoveRect(i).Contains(x, y)) {
            pins.erase(pins.begin() + i);
            if (focus == Focus::PinName) {
                focus = Focus::None;
                focusedPinIndex = -1;
            }
            return;
        }
    }
    if (AddPinButtonRect().Contains(x, y)) {
        // New rows start as a copy of the previous row (with a unique
        // name when named) so similar pins need only a small edit.
        PinDef pin;
        if (pins.empty()) {
            pin.direction = PinDirection::Input;
            pin.type = PinType::Float;
        } else {
            pin = pins.back();
            pin.name = MakeUniquePinName(pin.name, pins);
        }
        pins.push_back(std::move(pin));
        return;
    }
    for (int i = 0; i < static_cast<int>(properties.size()); ++i) {
        PropertyDraft& property = properties[static_cast<std::size_t>(i)];
        if (PropertyContainerRect(i).Contains(x, y)) {
            OpenDropdown(DialogDropdownKind::PropertyContainer, i);
            return;
        }
        if (PropertyTypeRect(i).Contains(x, y)) {
            OpenDropdown(DialogDropdownKind::PropertyType, i);
            return;
        }
        if (PropertyKeyTypeRect(i).Contains(x, y)) {
            if (property.container == PropertyContainer::Map) {
                OpenDropdown(DialogDropdownKind::PropertyKeyType, i);
            }
            return;
        }
        if (PropertyNameRect(i).Contains(x, y)) {
            focus = Focus::PropertyName;
            focusedPropertyIndex = i;
            focusedPinIndex = -1;
            return;
        }
        if (PropertyDefaultRect(i).Contains(x, y)) {
            focus = Focus::PropertyDefault;
            focusedPropertyIndex = i;
            focusedPinIndex = -1;
            return;
        }
        if (PropertyRemoveRect(i).Contains(x, y)) {
            properties.erase(properties.begin() + i);
            if (focus == Focus::PropertyName || focus == Focus::PropertyDefault) {
                focus = Focus::None;
                focusedPropertyIndex = -1;
            }
            return;
        }
    }
    if (AddPropertyButtonRect().Contains(x, y)) {
        // New rows start as a copy of the previous row (with a unique
        // name) so sequences like x/y/z need only a small edit.
        PropertyDraft draft;
        if (!properties.empty()) {
            draft = properties.back();
            draft.name = MakeUniqueDraftName(draft.name, properties);
        }
        properties.push_back(std::move(draft));
        return;
    }
    focus = Focus::None;
    focusedPinIndex = -1;
    focusedPropertyIndex = -1;
}

void ClassEditorDialog::LoadExistingType(int registryIndex)
{
    const std::vector<UserType>& all = UserTypeRegistry::GetAll();
    if (registryIndex < 0 || registryIndex >= static_cast<int>(all.size())) {
        return;
    }
    const UserType& type = all[static_cast<std::size_t>(registryIndex)];
    typeNameText = type.name;
    typeKind = type.kind;
    enumValues = type.enumerators;
    structFields = type.fields;
    typeEditOldName = type.name;
    focus = Focus::None;
    focusedEnumIndex = -1;
    errorText.clear();
}

void ClassEditorDialog::SwitchMode(DialogEditMode newMode)
{
    if (mode == newMode) {
        return;
    }
    mode = newMode;
    focus = Focus::None;
    focusedPinIndex = -1;
    focusedPropertyIndex = -1;
    focusedEnumIndex = -1;
    errorText.clear();
    CloseDropdown();
    if (mode == DialogEditMode::Type && enumValues.empty()) {
        enumValues.push_back(std::string());
    }
}

void ClassEditorDialog::HandleTypeModeMouseDown(float x, float y)
{
    if (TypeNameFieldRect().Contains(x, y)) {
        focus = Focus::TypeName;
        focusedEnumIndex = -1;
        return;
    }
    if (LoadTypeRect().Contains(x, y)) {
        OpenDropdown(DialogDropdownKind::LoadType, -1);
        return;
    }
    if (TypeKindRect().Contains(x, y)) {
        OpenDropdown(DialogDropdownKind::TypeKind, -1);
        return;
    }

    if (typeKind == UserTypeKind::Enum) {
        for (int i = 0; i < static_cast<int>(enumValues.size()); ++i) {
            if (EnumValueRect(i).Contains(x, y)) {
                focus = Focus::EnumValue;
                focusedEnumIndex = i;
                return;
            }
            if (EnumValueRemoveRect(i).Contains(x, y)) {
                enumValues.erase(enumValues.begin() + i);
                if (focus == Focus::EnumValue) {
                    focus = Focus::None;
                    focusedEnumIndex = -1;
                }
                return;
            }
        }
        if (AddEnumValueButtonRect().Contains(x, y)) {
            enumValues.push_back(std::string());
            return;
        }
    } else if (typeKind == UserTypeKind::Struct) {
        for (int i = 0; i < static_cast<int>(structFields.size()); ++i) {
            if (FieldNameRect(i).Contains(x, y)) {
                focus = Focus::StructFieldName;
                focusedEnumIndex = i;
                return;
            }
            if (FieldTypeRect(i).Contains(x, y)) {
                OpenDropdown(DialogDropdownKind::FieldType, i);
                return;
            }
            if (EnumValueRemoveRect(i).Contains(x, y)) {
                structFields.erase(structFields.begin() + i);
                if (focus == Focus::StructFieldName) {
                    focus = Focus::None;
                    focusedEnumIndex = -1;
                }
                return;
            }
        }
        if (AddEnumValueButtonRect().Contains(x, y)) {
            StructField field;
            field.type = PinType::Float;
            structFields.push_back(std::move(field));
            return;
        }
    }

    focus = Focus::None;
    focusedEnumIndex = -1;
}

void ClassEditorDialog::AppendText(const char* text)
{
    if (focus == Focus::TypeName) {
        if (typeNameText.size() < MAX_CLASS_NAME_LENGTH) {
            typeNameText += text;
        }
        return;
    }
    if (focus == Focus::EnumValue
        && focusedEnumIndex >= 0
        && focusedEnumIndex < static_cast<int>(enumValues.size())) {
        std::string& value = enumValues[static_cast<std::size_t>(focusedEnumIndex)];
        if (value.size() < MAX_PIN_NAME_LENGTH) {
            value += text;
        }
        return;
    }
    if (focus == Focus::StructFieldName
        && focusedEnumIndex >= 0
        && focusedEnumIndex < static_cast<int>(structFields.size())) {
        std::string& name = structFields[static_cast<std::size_t>(focusedEnumIndex)].name;
        if (name.size() < MAX_PIN_NAME_LENGTH) {
            name += text;
        }
        return;
    }
    if (focus == Focus::ClassName) {
        if (classNameText.size() < MAX_CLASS_NAME_LENGTH) {
            classNameText += text;
        }
        return;
    }
    if (focus == Focus::Category) {
        if (categoryText.size() < MAX_CATEGORY_LENGTH) {
            categoryText += text;
        }
        return;
    }
    if (focus == Focus::PinName
        && focusedPinIndex >= 0
        && focusedPinIndex < static_cast<int>(pins.size())) {
        std::string& name = pins[static_cast<std::size_t>(focusedPinIndex)].name;
        if (name.size() < MAX_PIN_NAME_LENGTH) {
            name += text;
        }
        return;
    }
    if (focusedPropertyIndex >= 0
        && focusedPropertyIndex < static_cast<int>(properties.size())) {
        PropertyDraft& property = properties[static_cast<std::size_t>(focusedPropertyIndex)];
        if (focus == Focus::PropertyName && property.name.size() < MAX_PIN_NAME_LENGTH) {
            property.name += text;
        } else if (focus == Focus::PropertyDefault
                   && property.defaultText.size() < MAX_DEFAULT_TEXT_LENGTH) {
            property.defaultText += text;
        }
    }
}

void ClassEditorDialog::HandleBackspace()
{
    if (focus == Focus::TypeName) {
        PopLastUTF8Character(typeNameText);
        return;
    }
    if (focus == Focus::EnumValue
        && focusedEnumIndex >= 0
        && focusedEnumIndex < static_cast<int>(enumValues.size())) {
        PopLastUTF8Character(enumValues[static_cast<std::size_t>(focusedEnumIndex)]);
        return;
    }
    if (focus == Focus::StructFieldName
        && focusedEnumIndex >= 0
        && focusedEnumIndex < static_cast<int>(structFields.size())) {
        PopLastUTF8Character(structFields[static_cast<std::size_t>(focusedEnumIndex)].name);
        return;
    }
    if (focus == Focus::ClassName) {
        PopLastUTF8Character(classNameText);
        return;
    }
    if (focus == Focus::Category) {
        PopLastUTF8Character(categoryText);
        return;
    }
    if (focus == Focus::PinName
        && focusedPinIndex >= 0
        && focusedPinIndex < static_cast<int>(pins.size())) {
        PopLastUTF8Character(pins[static_cast<std::size_t>(focusedPinIndex)].name);
        return;
    }
    if (focusedPropertyIndex >= 0
        && focusedPropertyIndex < static_cast<int>(properties.size())) {
        PropertyDraft& property = properties[static_cast<std::size_t>(focusedPropertyIndex)];
        if (focus == Focus::PropertyName) {
            PopLastUTF8Character(property.name);
        } else if (focus == Focus::PropertyDefault) {
            PopLastUTF8Character(property.defaultText);
        }
    }
}

// Converts one property draft to a PropertyDef, parsing the default text
// per container shape. Returns false and sets outError on bad input.
static bool ConvertPropertyDraft(const PropertyDraft& draft, PropertyDef& outDef,
                                 std::string& outError)
{
    outDef.name = TrimAscii(draft.name);
    outDef.container = draft.container;
    outDef.type = draft.type;
    outDef.typeName = draft.typeName;
    outDef.keyType = draft.keyType;
    outDef.keyTypeName = draft.keyTypeName;
    if (outDef.name.empty()) {
        outError = "Property name is empty";
        return false;
    }
    if (outDef.type == PinType::UserType && outDef.container != PropertyContainer::None) {
        const UserType* elementType = UserTypeRegistry::Find(outDef.typeName);
        if (elementType != nullptr && elementType->kind == UserTypeKind::Struct) {
            outError = "Struct type is only allowed on a scalar property: " + outDef.name;
            return false;
        }
    }
    if (outDef.container == PropertyContainer::Map && outDef.keyType == PinType::UserType) {
        const UserType* keyType = UserTypeRegistry::Find(outDef.keyTypeName);
        if (keyType != nullptr && keyType->kind == UserTypeKind::Struct) {
            outError = "Struct cannot be a map key: " + outDef.name;
            return false;
        }
    }

    const std::string text = TrimAscii(draft.defaultText);
    switch (draft.container) {
    case PropertyContainer::None:
        if (!ParseValueString(text, draft.type, outDef.defaultValue)) {
            outError = "Invalid default for property: " + outDef.name;
            return false;
        }
        return true;

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        if (text.empty()) {
            return true;
        }
        for (const std::string& part : SplitTrimmed(text, ',')) {
            Value element;
            if (!ParseValueString(part, draft.type, element)) {
                outError = "Invalid element '" + part + "' in property: " + outDef.name;
                return false;
            }
            if (draft.container == PropertyContainer::Set) {
                bool duplicate = false;
                for (const Value& existing : outDef.defaultElements) {
                    if (existing == element) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    continue;
                }
            }
            outDef.defaultElements.push_back(std::move(element));
        }
        return true;
    }

    case PropertyContainer::Map: {
        if (text.empty()) {
            return true;
        }
        for (const std::string& part : SplitTrimmed(text, ',')) {
            const std::size_t separator = part.find(':');
            if (separator == std::string::npos) {
                outError = "Map entry '" + part + "' must be key:value in property: " + outDef.name;
                return false;
            }
            Value key;
            if (!ParseValueString(TrimAscii(part.substr(0, separator)), draft.keyType, key)) {
                outError = "Invalid map key in '" + part + "' in property: " + outDef.name;
                return false;
            }
            for (const std::pair<Value, Value>& existing : outDef.defaultEntries) {
                if (existing.first == key) {
                    outError = "Duplicate map key in property: " + outDef.name;
                    return false;
                }
            }
            Value value;
            if (!ParseValueString(TrimAscii(part.substr(separator + 1)), draft.type, value)) {
                outError = "Invalid map value in '" + part + "' in property: " + outDef.name;
                return false;
            }
            outDef.defaultEntries.emplace_back(std::move(key), std::move(value));
        }
        return true;
    }
    }
    outError = "Unsupported container in property: " + outDef.name;
    return false;
}

ClassEditorAction ClassEditorDialog::TrySubmit()
{
    ClassEditorAction action;

    const std::string trimmedName = TrimAscii(classNameText);
    if (trimmedName.empty()) {
        errorText = "Class name is empty";
        return action;
    }
    const NodeClass* existing = NodeClass::FindByName(trimmedName.c_str());
    if (existing != nullptr && existing != editTarget) {
        errorText = "Duplicate class name: " + trimmedName;
        return action;
    }
    const std::string trimmedCategory = TrimAscii(categoryText);
    if (trimmedCategory.empty()) {
        errorText = "Category is empty";
        return action;
    }

    std::vector<PropertyDef> propertyDefs;
    for (const PropertyDraft& draft : properties) {
        PropertyDef def;
        std::string convertError;
        if (!ConvertPropertyDraft(draft, def, convertError)) {
            errorText = convertError;
            return action;
        }
        for (const PropertyDef& existing : propertyDefs) {
            if (existing.name == def.name) {
                errorText = "Duplicate property name: " + def.name;
                return action;
            }
        }
        propertyDefs.push_back(std::move(def));
    }

    action.type = ClassEditorAction::Type::Submit;
    action.name = trimmedName;
    action.category = trimmedCategory;
    action.pins = pins;
    for (const PinDef& execOut : keptExecOutputs) {
        action.pins.push_back(execOut);
    }
    action.properties = std::move(propertyDefs);
    action.editTarget = editTarget;
    Close();
    return action;
}

ClassEditorAction ClassEditorDialog::TrySubmitType()
{
    ClassEditorAction action;

    const std::string trimmed = TrimAscii(typeNameText);
    if (trimmed.empty()) {
        errorText = "Type name is empty";
        return action;
    }
    PinType builtinCollision;
    if (PinTypeFromString(trimmed, builtinCollision)) {
        errorText = "Name collides with a builtin type";
        return action;
    }

    UserType type;
    type.name = trimmed;
    type.kind = typeKind;
    if (typeKind == UserTypeKind::Enum) {
        for (const std::string& value : enumValues) {
            const std::string trimmedValue = TrimAscii(value);
            if (trimmedValue.empty()) {
                continue;
            }
            for (const std::string& existing : type.enumerators) {
                if (existing == trimmedValue) {
                    errorText = "Duplicate enum value: " + trimmedValue;
                    return action;
                }
            }
            type.enumerators.push_back(trimmedValue);
        }
        if (type.enumerators.empty()) {
            errorText = "Enum needs at least one value";
            return action;
        }
    } else if (typeKind == UserTypeKind::Struct) {
        for (const StructField& field : structFields) {
            StructField clean = field;
            clean.name = TrimAscii(field.name);
            if (clean.name.empty()) {
                errorText = "Struct field name is empty";
                return action;
            }
            for (const StructField& existing : type.fields) {
                if (existing.name == clean.name) {
                    errorText = "Duplicate struct field: " + clean.name;
                    return action;
                }
            }
            type.fields.push_back(std::move(clean));
        }
        if (type.fields.empty()) {
            errorText = "Struct needs at least one field";
            return action;
        }
    }

    action.type = ClassEditorAction::Type::SubmitType;
    action.userType = std::move(type);
    action.typeEditOldName = typeEditOldName;
    Close();
    return action;
}
