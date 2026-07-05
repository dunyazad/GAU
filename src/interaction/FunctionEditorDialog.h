#pragma once

#include "EditorInputEvent.h"
#include "UIRect.h"
#include "UIScale.h"

#include <cstddef>
#include <string>

struct FunctionEditorAction
{
    enum class Type
    {
        None,
        // Build & Save pressed: write the source and compile it.
        Build,
        Closed,
    };

    Type type = Type::None;
    std::string name;
    std::string source;
};

// Modal editor for a custom wasm node function: function name and C++
// source with caret/selection editing (mouse click/drag, shift+arrows,
// Delete, Ctrl+A; clipboard operations are routed through main which
// owns the OS clipboard access). The build itself (clang invocation,
// class registration, module reload) happens in main; SetStatus reports
// the result back into the dialog.
class FunctionEditorDialog
{
public:
    static constexpr float WIDTH = 560.0f * UI_SCALE;
    static constexpr float PADDING = 10.0f * UI_SCALE;
    static constexpr float TITLE_HEIGHT = 22.0f * UI_SCALE;
    static constexpr float ROW_HEIGHT = 26.0f * UI_SCALE;
    static constexpr float GAP = 8.0f * UI_SCALE;
    static constexpr float LINE_HEIGHT = 16.0f * UI_SCALE;
    static constexpr float TEXT_INSET = 6.0f * UI_SCALE;
    static constexpr int SOURCE_VISIBLE_LINES = 18;
    static constexpr float STATUS_HEIGHT = 18.0f * UI_SCALE;
    static constexpr float BUTTON_WIDTH = 110.0f * UI_SCALE;
    static constexpr float BUTTON_HEIGHT = 28.0f * UI_SCALE;

    enum class Focus
    {
        Name,
        Source,
    };

    bool IsOpen() const { return open; }

    void Open(float screenWidth, float screenHeight);
    void Close();

    // Consumes all events while open.
    FunctionEditorAction HandleEvent(const EditorInputEvent& event);

    void SetStatus(const std::string& text) { statusText = text; }
    // Advance width of one monospace glyph at the editor font size;
    // measured by the render layer once at startup (mouse caret math).
    void SetCharWidth(float width) { charWidth = width; }
    float GetCharWidth() const { return charWidth; }

    // Clipboard bridge (main owns the OS clipboard).
    std::string GetSelectedText() const;
    void DeleteSelectedText();
    void InsertAtCaret(const std::string& text);
    bool HasSelection() const { return selectionAnchor != NO_SELECTION && selectionAnchor != caretIndex; }
    void GetSelectionRange(std::size_t& outBegin, std::size_t& outEnd) const;

    float GetX() const { return panelX; }
    float GetY() const { return panelY; }
    float GetPanelHeight() const;
    const std::string& GetFunctionName() const { return functionName; }
    const std::string& GetSource() const { return source; }
    const std::string& GetStatusText() const { return statusText; }
    Focus GetFocus() const { return focus; }
    std::size_t GetCaretIndex() const { return caretIndex; }
    int GetFirstVisibleLine() const { return firstVisibleLine; }
    // Caret position as line/column (character counts).
    void GetCaretLineColumn(int& outLine, int& outColumn) const;
    void GetLineColumnAt(std::size_t index, int& outLine, int& outColumn) const;

    UIRect NameFieldRect() const;
    UIRect SourceRect() const;
    UIRect BuildButtonRect() const;
    UIRect CancelButtonRect() const;
    // Grab strip at the top of the panel: dragging it moves the window.
    UIRect TitleBarRect() const;

private:
    static constexpr std::size_t NO_SELECTION = static_cast<std::size_t>(-1);

    void InsertText(const char* text);
    void MoveCaretVertical(int lineDelta, bool extendSelection);
    void BeginOrClearSelection(bool extendSelection);
    void EnsureCaretVisible();
    std::size_t LineStartIndex(int line) const;
    int LineCount() const;
    int CaretLine() const;
    std::size_t IndexFromMouse(float x, float y) const;
    void HandleSourceKey(const EditorInputEvent& event);

    bool open = false;
    float panelX = 0.0f;
    float panelY = 0.0f;
    // Title-bar drag state (window move).
    bool draggingTitle = false;
    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    Focus focus = Focus::Source;
    std::string functionName;
    std::string source;
    std::size_t caretIndex = 0;
    std::size_t selectionAnchor = NO_SELECTION;
    bool mouseSelecting = false;
    int firstVisibleLine = 0;
    float charWidth = 7.0f * UI_SCALE;
    std::string statusText;
};
