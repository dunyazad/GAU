#include "ClassEditorDialog.h"

#include <cctype>

static const std::size_t MAX_CLASS_NAME_LENGTH = 48;
static const std::size_t MAX_PIN_NAME_LENGTH = 32;

static NodeCategory NextCategory(NodeCategory category)
{
    const int next = (NodeCategoryIndex(category) + 1) % NODE_CATEGORY_COUNT;
    return ALL_NODE_CATEGORIES[next];
}

static PinType NextPinType(PinType type)
{
    for (int i = 0; i < PIN_TYPE_COUNT; ++i) {
        if (ALL_PIN_TYPES[i] == type) {
            return ALL_PIN_TYPES[(i + 1) % PIN_TYPE_COUNT];
        }
    }
    return PinType::Exec;
}

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
    category = NodeCategory::Function;
    classNameText.clear();
    pins.clear();

    PinDef defaultPin;
    defaultPin.direction = PinDirection::Input;
    defaultPin.type = PinType::Exec;
    pins.push_back(defaultPin);

    focus = Focus::ClassName;
    focusedPinIndex = -1;
    errorText.clear();

    panelX = (screenWidth - WIDTH) * 0.5f;
    panelY = (screenHeight - GetPanelHeight()) * 0.5f;
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
}

void ClassEditorDialog::Close()
{
    open = false;
    classNameText.clear();
    pins.clear();
    errorText.clear();
    focus = Focus::None;
    focusedPinIndex = -1;
}

float ClassEditorDialog::GetPanelHeight() const
{
    return PADDING + TITLE_HEIGHT + GAP
         + ROW_HEIGHT + GAP
         + ROW_HEIGHT + GAP
         + PINS_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(pins.size())
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

float ClassEditorDialog::PinRowY(int pinIndex) const
{
    return CategoryRowY() + ROW_HEIGHT + GAP + PINS_LABEL_HEIGHT
         + PIN_ROW_STRIDE * static_cast<float>(pinIndex);
}

float ClassEditorDialog::AddPinRowY() const
{
    return PinRowY(static_cast<int>(pins.size())) + GAP;
}

float ClassEditorDialog::ErrorRowY() const
{
    return AddPinRowY() + BUTTON_HEIGHT + GAP;
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

UIRect ClassEditorDialog::CategoryButtonRect() const
{
    return UIRect{panelX + PADDING + LABEL_WIDTH, CategoryRowY(), 140.0f, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinDirectionRect(int pinIndex) const
{
    return UIRect{panelX + PADDING, PinRowY(pinIndex), 46.0f, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinTypeRect(int pinIndex) const
{
    return UIRect{panelX + PADDING + 52.0f, PinRowY(pinIndex), 64.0f, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinNameRect(int pinIndex) const
{
    const float left = panelX + PADDING + 122.0f;
    const float right = panelX + WIDTH - PADDING - 28.0f;
    return UIRect{left, PinRowY(pinIndex), right - left, ROW_HEIGHT};
}

UIRect ClassEditorDialog::PinRemoveRect(int pinIndex) const
{
    return UIRect{panelX + WIDTH - PADDING - 22.0f, PinRowY(pinIndex), 22.0f, ROW_HEIGHT};
}

UIRect ClassEditorDialog::AddPinButtonRect() const
{
    return UIRect{panelX + PADDING, AddPinRowY(), 100.0f, BUTTON_HEIGHT};
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

ClassEditorAction ClassEditorDialog::HandleEvent(const EditorInputEvent& event)
{
    ClassEditorAction action;
    if (!open) {
        return action;
    }

    switch (event.type) {
    case EditorInputType::MouseDown:
        if (event.button == EditorMouseButton::Left) {
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
            Close();
            action.type = ClassEditorAction::Type::Closed;
        } else if (event.key == EditorKey::Enter) {
            return TrySubmit();
        } else if (event.key == EditorKey::Backspace) {
            HandleBackspace();
        }
        break;

    case EditorInputType::TextInput:
        AppendText(event.text);
        break;

    case EditorInputType::MouseMove:
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
        return;
    }
    if (CategoryButtonRect().Contains(x, y)) {
        category = NextCategory(category);
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
            pin.type = NextPinType(pin.type);
            return;
        }
        if (PinNameRect(i).Contains(x, y)) {
            focus = Focus::PinName;
            focusedPinIndex = i;
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
        PinDef pin;
        pin.direction = PinDirection::Input;
        pin.type = PinType::Exec;
        pins.push_back(pin);
        return;
    }
    focus = Focus::None;
    focusedPinIndex = -1;
}

void ClassEditorDialog::AppendText(const char* text)
{
    if (focus == Focus::ClassName) {
        if (classNameText.size() < MAX_CLASS_NAME_LENGTH) {
            classNameText += text;
        }
    } else if (focus == Focus::PinName
               && focusedPinIndex >= 0
               && focusedPinIndex < static_cast<int>(pins.size())) {
        std::string& name = pins[static_cast<std::size_t>(focusedPinIndex)].name;
        if (name.size() < MAX_PIN_NAME_LENGTH) {
            name += text;
        }
    }
}

void ClassEditorDialog::HandleBackspace()
{
    if (focus == Focus::ClassName) {
        PopLastUTF8Character(classNameText);
    } else if (focus == Focus::PinName
               && focusedPinIndex >= 0
               && focusedPinIndex < static_cast<int>(pins.size())) {
        PopLastUTF8Character(pins[static_cast<std::size_t>(focusedPinIndex)].name);
    }
}

ClassEditorAction ClassEditorDialog::TrySubmit()
{
    ClassEditorAction action;

    const std::string trimmedName = TrimAscii(classNameText);
    if (trimmedName.empty()) {
        errorText = "Class name is empty";
        return action;
    }
    if (NodeClass::FindByName(trimmedName.c_str()) != nullptr) {
        errorText = "Duplicate class name: " + trimmedName;
        return action;
    }

    action.type = ClassEditorAction::Type::Submit;
    action.name = trimmedName;
    action.category = category;
    action.pins = pins;
    Close();
    return action;
}
