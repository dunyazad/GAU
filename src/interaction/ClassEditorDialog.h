#pragma once

#include "EditorInputEvent.h"

#include "model/GraphTypes.h"
#include "model/NodeClass.h"

#include <string>
#include <vector>

struct UIRect
{
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;

    bool Contains(float px, float py) const
    {
        return px >= x && px <= x + w && py >= y && py <= y + h;
    }
};

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
    NodeCategory category = NodeCategory::Function;
    std::vector<PinDef> pins;
};

// Modal dialog for registering a custom NodeClass at runtime: class name
// text field, category cycle button, editable pin rows (direction/type
// cycle buttons, name field, remove), add-pin button and Create/Cancel.
// Owns state and hit testing only; drawing lives in
// render/ClassEditorDialogRenderer, which shares the rect getters below.
class ClassEditorDialog
{
public:
    static constexpr float WIDTH = 420.0f;
    static constexpr float PADDING = 12.0f;
    static constexpr float TITLE_HEIGHT = 22.0f;
    static constexpr float ROW_HEIGHT = 26.0f;
    static constexpr float GAP = 8.0f;
    static constexpr float LABEL_WIDTH = 80.0f;
    static constexpr float PINS_LABEL_HEIGHT = 20.0f;
    static constexpr float PIN_ROW_STRIDE = 30.0f;
    static constexpr float ERROR_HEIGHT = 18.0f;
    static constexpr float BUTTON_WIDTH = 96.0f;
    static constexpr float BUTTON_HEIGHT = 28.0f;

    enum class Focus
    {
        None,
        ClassName,
        PinName,
    };

    bool IsOpen() const { return open; }

    // Opens centered, with one default exec input pin.
    void Open(float screenWidth, float screenHeight);
    void Close();

    // Handles one input event while open; the dialog consumes all events.
    ClassEditorAction HandleEvent(const EditorInputEvent& event);

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::string& GetClassNameText() const { return classNameText; }
    NodeCategory GetCategory() const { return category; }
    const std::vector<PinDef>& GetPins() const { return pins; }
    Focus GetFocus() const { return focus; }
    int GetFocusedPinIndex() const { return focusedPinIndex; }
    const std::string& GetErrorText() const { return errorText; }

    // Layout rects shared by hit testing and the renderer.
    UIRect NameFieldRect() const;
    UIRect CategoryButtonRect() const;
    UIRect PinDirectionRect(int pinIndex) const;
    UIRect PinTypeRect(int pinIndex) const;
    UIRect PinNameRect(int pinIndex) const;
    UIRect PinRemoveRect(int pinIndex) const;
    UIRect AddPinButtonRect() const;
    UIRect OkButtonRect() const;
    UIRect CancelButtonRect() const;

private:
    float NameRowY() const;
    float CategoryRowY() const;
    float PinRowY(int pinIndex) const;
    float AddPinRowY() const;
    float ErrorRowY() const;
    float ButtonRowY() const;

    void HandleMouseDown(float x, float y);
    void AppendText(const char* text);
    void HandleBackspace();
    ClassEditorAction TrySubmit();

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    NodeCategory category = NodeCategory::Function;
    std::string classNameText;
    std::vector<PinDef> pins;
    Focus focus = Focus::None;
    int focusedPinIndex = -1;
    std::string errorText;
};
