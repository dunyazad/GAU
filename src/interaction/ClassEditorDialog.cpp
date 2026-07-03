#include "ClassEditorDialog.h"

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
    categoryText = "Function";
    classNameText.clear();
    pins.clear();
    properties.clear();

    PinDef defaultPin;
    defaultPin.direction = PinDirection::Input;
    defaultPin.type = PinType::Exec;
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
    pins = target.GetPins();

    properties.clear();
    for (const PropertyDef& def : target.GetProperties()) {
        PropertyDraft draft;
        draft.container = def.container;
        draft.type = def.type;
        draft.keyType = def.keyType;
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
    editTarget = nullptr;
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
    return PADDING + TITLE_HEIGHT + GAP
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

float ClassEditorDialog::NameRowY() const
{
    return panelY + PADDING + TITLE_HEIGHT + GAP;
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

int ClassEditorDialog::GetDropdownOptionCount() const
{
    switch (dropdownKind) {
    case DialogDropdownKind::None:
        return 0;
    case DialogDropdownKind::Category:
        return static_cast<int>(categoryOptions.size());
    case DialogDropdownKind::PinType:
        return PIN_TYPE_COUNT;
    case DialogDropdownKind::PropertyContainer:
        return PROPERTY_CONTAINER_COUNT;
    case DialogDropdownKind::PropertyType:
    case DialogDropdownKind::PropertyKeyType:
        return VALUE_PIN_TYPE_COUNT;
    }
    return 0;
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
            const PinType type = pins[static_cast<std::size_t>(dropdownRowIndex)].type;
            for (int i = 0; i < PIN_TYPE_COUNT; ++i) {
                if (ALL_PIN_TYPES[i] == type) {
                    return i;
                }
            }
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
            return ValuePinTypeIndex(properties[static_cast<std::size_t>(dropdownRowIndex)].type);
        }
        return -1;
    case DialogDropdownKind::PropertyKeyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            return ValuePinTypeIndex(properties[static_cast<std::size_t>(dropdownRowIndex)].keyType);
        }
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

void ClassEditorDialog::OpenDropdown(DialogDropdownKind kind, int rowIndex)
{
    dropdownKind = kind;
    dropdownRowIndex = rowIndex;
    dropdownHoverIndex = -1;

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
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(pins.size())) {
            pins[static_cast<std::size_t>(dropdownRowIndex)].type = ALL_PIN_TYPES[optionIndex];
        }
        break;
    case DialogDropdownKind::PropertyContainer:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            properties[static_cast<std::size_t>(dropdownRowIndex)].container =
                ALL_PROPERTY_CONTAINERS[optionIndex];
        }
        break;
    case DialogDropdownKind::PropertyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            properties[static_cast<std::size_t>(dropdownRowIndex)].type =
                VALUE_PIN_TYPES[optionIndex];
        }
        break;
    case DialogDropdownKind::PropertyKeyType:
        if (dropdownRowIndex >= 0 && dropdownRowIndex < static_cast<int>(properties.size())) {
            properties[static_cast<std::size_t>(dropdownRowIndex)].keyType =
                VALUE_PIN_TYPES[optionIndex];
        }
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
            if (OkButtonRect().Contains(event.x, event.y)) {
                return TrySubmit();
            }
            if (CancelButtonRect().Contains(event.x, event.y)) {
                Close();
                action.type = ClassEditorAction::Type::Closed;
                return action;
            }
            HandleMouseDown(event.x, event.y);
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
                return TrySubmit();
            }
        } else if (event.key == EditorKey::Backspace) {
            HandleBackspace();
        }
        break;

    case EditorInputType::TextInput:
        AppendText(event.text);
        break;

    case EditorInputType::MouseUp:
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
            pin.type = PinType::Exec;
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

void ClassEditorDialog::AppendText(const char* text)
{
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
    outDef.keyType = draft.keyType;
    if (outDef.name.empty()) {
        outError = "Property name is empty";
        return false;
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
    action.properties = std::move(propertyDefs);
    action.editTarget = editTarget;
    Close();
    return action;
}
