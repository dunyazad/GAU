#include "FunctionEditorDialog.h"

static const std::size_t MAX_SOURCE_LENGTH = 65536;
static const std::size_t MAX_NAME_LENGTH = 48;

static const char* DEFAULT_SOURCE =
    "// GAU wasm node function (C++, no exceptions/RTTI, no stdlib).\n"
    "// The directives below define the node class (created/updated on\n"
    "// build). Pin tokens are name:type; use _ for unnamed exec pins.\n"
    "// @node category=Function\n"
    "// @in a:int b:int\n"
    "// @out result:int\n"
    "\n"
    "// gau_api.h is generated from the node classes: host imports plus\n"
    "// structs and gau_read_/gau_write_ helpers for data classes.\n"
    "#include \"gau_api.h\"\n"
    "\n"
    "extern \"C\" void my_function(void)\n"
    "{\n"
    "    gau_output_i32(0, gau_input_i32(0) + gau_input_i32(1));\n"
    "}\n";

void FunctionEditorDialog::Open(float screenWidth, float screenHeight)
{
    open = true;
    functionName = "my_function";
    source = DEFAULT_SOURCE;
    caretIndex = source.size();
    selectionAnchor = NO_SELECTION;
    mouseSelecting = false;
    firstVisibleLine = 0;
    focus = Focus::Source;
    statusText.clear();

    panelX = (screenWidth - WIDTH) * 0.5f;
    panelY = (screenHeight - GetPanelHeight()) * 0.5f;
    if (panelX < 0.0f) {
        panelX = 0.0f;
    }
    if (panelY < 0.0f) {
        panelY = 0.0f;
    }
    EnsureCaretVisible();
}

void FunctionEditorDialog::Close()
{
    open = false;
    source.clear();
    functionName.clear();
    statusText.clear();
    caretIndex = 0;
    selectionAnchor = NO_SELECTION;
    mouseSelecting = false;
    draggingTitle = false;
    firstVisibleLine = 0;
}

float FunctionEditorDialog::GetPanelHeight() const
{
    return PADDING + TITLE_HEIGHT + GAP
         + ROW_HEIGHT + GAP
         + LINE_HEIGHT * static_cast<float>(SOURCE_VISIBLE_LINES) + GAP
         + STATUS_HEIGHT + GAP
         + BUTTON_HEIGHT + PADDING;
}

UIRect FunctionEditorDialog::NameFieldRect() const
{
    const float labelWidth = 70.0f * UI_SCALE;
    return UIRect{panelX + PADDING + labelWidth, panelY + PADDING + TITLE_HEIGHT + GAP,
                  WIDTH - PADDING * 2.0f - labelWidth, ROW_HEIGHT};
}

UIRect FunctionEditorDialog::SourceRect() const
{
    const UIRect name = NameFieldRect();
    return UIRect{panelX + PADDING, name.y + name.h + GAP,
                  WIDTH - PADDING * 2.0f,
                  LINE_HEIGHT * static_cast<float>(SOURCE_VISIBLE_LINES)};
}

UIRect FunctionEditorDialog::BuildButtonRect() const
{
    const UIRect sourceRect = SourceRect();
    const float y = sourceRect.y + sourceRect.h + GAP + STATUS_HEIGHT + GAP;
    return UIRect{panelX + WIDTH - PADDING - BUTTON_WIDTH * 2.0f - GAP, y,
                  BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect FunctionEditorDialog::CancelButtonRect() const
{
    const UIRect build = BuildButtonRect();
    return UIRect{build.x + build.w + GAP, build.y, BUTTON_WIDTH, BUTTON_HEIGHT};
}

UIRect FunctionEditorDialog::TitleBarRect() const
{
    return UIRect{panelX, panelY, WIDTH, PADDING + TITLE_HEIGHT};
}

int FunctionEditorDialog::LineCount() const
{
    int count = 1;
    for (char c : source) {
        if (c == '\n') {
            ++count;
        }
    }
    return count;
}

int FunctionEditorDialog::CaretLine() const
{
    int line = 0;
    for (std::size_t i = 0; i < caretIndex && i < source.size(); ++i) {
        if (source[i] == '\n') {
            ++line;
        }
    }
    return line;
}

std::size_t FunctionEditorDialog::LineStartIndex(int line) const
{
    if (line <= 0) {
        return 0;
    }
    int currentLine = 0;
    for (std::size_t i = 0; i < source.size(); ++i) {
        if (source[i] == '\n') {
            ++currentLine;
            if (currentLine == line) {
                return i + 1;
            }
        }
    }
    return source.size();
}

void FunctionEditorDialog::GetLineColumnAt(std::size_t index, int& outLine,
                                           int& outColumn) const
{
    outLine = 0;
    std::size_t lineStart = 0;
    for (std::size_t i = 0; i < index && i < source.size(); ++i) {
        if (source[i] == '\n') {
            ++outLine;
            lineStart = i + 1;
        }
    }
    outColumn = static_cast<int>(index - lineStart);
}

void FunctionEditorDialog::GetCaretLineColumn(int& outLine, int& outColumn) const
{
    GetLineColumnAt(caretIndex, outLine, outColumn);
}

void FunctionEditorDialog::GetSelectionRange(std::size_t& outBegin, std::size_t& outEnd) const
{
    if (!HasSelection()) {
        outBegin = caretIndex;
        outEnd = caretIndex;
        return;
    }
    outBegin = (selectionAnchor < caretIndex) ? selectionAnchor : caretIndex;
    outEnd = (selectionAnchor < caretIndex) ? caretIndex : selectionAnchor;
}

std::string FunctionEditorDialog::GetSelectedText() const
{
    std::size_t begin = 0;
    std::size_t end = 0;
    GetSelectionRange(begin, end);
    return source.substr(begin, end - begin);
}

void FunctionEditorDialog::DeleteSelectedText()
{
    if (!HasSelection()) {
        return;
    }
    std::size_t begin = 0;
    std::size_t end = 0;
    GetSelectionRange(begin, end);
    source.erase(begin, end - begin);
    caretIndex = begin;
    selectionAnchor = NO_SELECTION;
    EnsureCaretVisible();
}

void FunctionEditorDialog::InsertAtCaret(const std::string& text)
{
    if (focus != Focus::Source) {
        return;
    }
    DeleteSelectedText();
    if (source.size() + text.size() > MAX_SOURCE_LENGTH) {
        return;
    }
    source.insert(caretIndex, text);
    caretIndex += text.size();
    EnsureCaretVisible();
}

void FunctionEditorDialog::EnsureCaretVisible()
{
    const int line = CaretLine();
    if (line < firstVisibleLine) {
        firstVisibleLine = line;
    }
    if (line >= firstVisibleLine + SOURCE_VISIBLE_LINES) {
        firstVisibleLine = line - SOURCE_VISIBLE_LINES + 1;
    }
}

void FunctionEditorDialog::InsertText(const char* text)
{
    if (focus == Focus::Name) {
        if (functionName.size() < MAX_NAME_LENGTH) {
            functionName += text;
        }
        return;
    }
    InsertAtCaret(std::string(text));
}

void FunctionEditorDialog::BeginOrClearSelection(bool extendSelection)
{
    if (extendSelection) {
        if (selectionAnchor == NO_SELECTION) {
            selectionAnchor = caretIndex;
        }
    } else {
        selectionAnchor = NO_SELECTION;
    }
}

void FunctionEditorDialog::MoveCaretVertical(int lineDelta, bool extendSelection)
{
    BeginOrClearSelection(extendSelection);

    int line = 0;
    int column = 0;
    GetCaretLineColumn(line, column);

    int targetLine = line + lineDelta;
    if (targetLine < 0) {
        targetLine = 0;
    }
    const int maxLine = LineCount() - 1;
    if (targetLine > maxLine) {
        targetLine = maxLine;
    }

    const std::size_t targetStart = LineStartIndex(targetLine);
    std::size_t targetEnd = targetStart;
    while (targetEnd < source.size() && source[targetEnd] != '\n') {
        ++targetEnd;
    }
    const int targetLength = static_cast<int>(targetEnd - targetStart);
    caretIndex = targetStart
               + static_cast<std::size_t>((column < targetLength) ? column : targetLength);
    EnsureCaretVisible();
}

std::size_t FunctionEditorDialog::IndexFromMouse(float x, float y) const
{
    const UIRect rect = SourceRect();
    int line = firstVisibleLine + static_cast<int>((y - rect.y) / LINE_HEIGHT);
    if (line < 0) {
        line = 0;
    }
    const int maxLine = LineCount() - 1;
    if (line > maxLine) {
        line = maxLine;
    }

    const float textX = x - rect.x - TEXT_INSET;
    int column = (charWidth > 0.0f)
                     ? static_cast<int>((textX + charWidth * 0.5f) / charWidth)
                     : 0;
    if (column < 0) {
        column = 0;
    }

    const std::size_t lineStart = LineStartIndex(line);
    std::size_t lineEnd = lineStart;
    while (lineEnd < source.size() && source[lineEnd] != '\n') {
        ++lineEnd;
    }
    const int lineLength = static_cast<int>(lineEnd - lineStart);
    if (column > lineLength) {
        column = lineLength;
    }
    return lineStart + static_cast<std::size_t>(column);
}

void FunctionEditorDialog::HandleSourceKey(const EditorInputEvent& event)
{
    switch (event.key) {
    case EditorKey::Backspace:
        if (HasSelection()) {
            DeleteSelectedText();
        } else if (caretIndex > 0) {
            std::size_t eraseBegin = caretIndex;
            while (eraseBegin > 0
                   && (static_cast<unsigned char>(source[eraseBegin - 1]) & 0xC0) == 0x80) {
                --eraseBegin;
            }
            if (eraseBegin > 0) {
                --eraseBegin;
            }
            source.erase(eraseBegin, caretIndex - eraseBegin);
            caretIndex = eraseBegin;
            EnsureCaretVisible();
        }
        break;

    case EditorKey::Delete:
        if (HasSelection()) {
            DeleteSelectedText();
        } else if (caretIndex < source.size()) {
            std::size_t eraseEnd = caretIndex + 1;
            while (eraseEnd < source.size()
                   && (static_cast<unsigned char>(source[eraseEnd]) & 0xC0) == 0x80) {
                ++eraseEnd;
            }
            source.erase(caretIndex, eraseEnd - caretIndex);
        }
        break;

    case EditorKey::Enter:
        InsertText("\n");
        break;

    case EditorKey::Tab:
        InsertText("    ");
        break;

    case EditorKey::Left:
        BeginOrClearSelection(event.shift);
        if (caretIndex > 0) {
            --caretIndex;
            EnsureCaretVisible();
        }
        break;

    case EditorKey::Right:
        BeginOrClearSelection(event.shift);
        if (caretIndex < source.size()) {
            ++caretIndex;
            EnsureCaretVisible();
        }
        break;

    case EditorKey::Up:
        MoveCaretVertical(-1, event.shift);
        break;

    case EditorKey::Down:
        MoveCaretVertical(1, event.shift);
        break;

    case EditorKey::Home:
        BeginOrClearSelection(event.shift);
        caretIndex = LineStartIndex(CaretLine());
        break;

    case EditorKey::End: {
        BeginOrClearSelection(event.shift);
        std::size_t end = caretIndex;
        while (end < source.size() && source[end] != '\n') {
            ++end;
        }
        caretIndex = end;
        break;
    }

    case EditorKey::SelectAll:
        selectionAnchor = 0;
        caretIndex = source.size();
        break;

    default:
        break;
    }
}

FunctionEditorAction FunctionEditorDialog::HandleEvent(const EditorInputEvent& event)
{
    FunctionEditorAction action;
    if (!open) {
        return action;
    }

    switch (event.type) {
    case EditorInputType::MouseDown:
        if (event.button == EditorMouseButton::Left) {
            if (BuildButtonRect().Contains(event.x, event.y)) {
                action.type = FunctionEditorAction::Type::Build;
                action.name = functionName;
                action.source = source;
                return action;
            }
            if (CancelButtonRect().Contains(event.x, event.y)) {
                Close();
                action.type = FunctionEditorAction::Type::Closed;
                return action;
            }
            if (TitleBarRect().Contains(event.x, event.y)) {
                draggingTitle = true;
                dragOffsetX = event.x - panelX;
                dragOffsetY = event.y - panelY;
                break;
            }
            if (NameFieldRect().Contains(event.x, event.y)) {
                focus = Focus::Name;
            } else if (SourceRect().Contains(event.x, event.y)) {
                focus = Focus::Source;
                const std::size_t index = IndexFromMouse(event.x, event.y);
                if (event.shift) {
                    if (selectionAnchor == NO_SELECTION) {
                        selectionAnchor = caretIndex;
                    }
                } else {
                    selectionAnchor = index;
                }
                caretIndex = index;
                mouseSelecting = true;
                EnsureCaretVisible();
            }
        }
        break;

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
        } else if (mouseSelecting) {
            caretIndex = IndexFromMouse(event.x, event.y);
            EnsureCaretVisible();
        }
        break;

    case EditorInputType::MouseUp:
        if (draggingTitle) {
            draggingTitle = false;
        } else if (mouseSelecting) {
            mouseSelecting = false;
            if (selectionAnchor == caretIndex) {
                selectionAnchor = NO_SELECTION;
            }
        }
        break;

    case EditorInputType::MouseWheel:
        if (SourceRect().Contains(event.x, event.y)) {
            firstVisibleLine -= static_cast<int>(event.wheelDelta) * 3;
            if (firstVisibleLine < 0) {
                firstVisibleLine = 0;
            }
            const int maxFirst = LineCount() - 1;
            if (firstVisibleLine > maxFirst) {
                firstVisibleLine = maxFirst;
            }
        }
        break;

    case EditorInputType::KeyDown:
        if (event.key == EditorKey::Escape) {
            Close();
            action.type = FunctionEditorAction::Type::Closed;
            return action;
        }
        if (focus == Focus::Name) {
            if (event.key == EditorKey::Backspace) {
                if (!functionName.empty()) {
                    functionName.pop_back();
                }
            } else if (event.key == EditorKey::Enter) {
                focus = Focus::Source;
            }
            break;
        }
        HandleSourceKey(event);
        break;

    case EditorInputType::TextInput:
        InsertText(event.text);
        break;
    }
    return action;
}
