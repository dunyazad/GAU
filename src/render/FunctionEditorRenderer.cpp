#include "FunctionEditorRenderer.h"

#include "interaction/FunctionEditorDialog.h"

#include <nanovg.h>

#include <string>
#include <vector>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const char* FONT_MONO = "mono";
static const float EDITOR_FONT_SIZE = 12.0f * UI_SCALE;

float MeasureMonoCharWidth(NVGcontext* vg, float fontSize)
{
    nvgFontFace(vg, FONT_MONO);
    nvgFontSize(vg, fontSize);
    float bounds[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    const float advance = nvgTextBounds(vg, 0.0f, 0.0f, "M", nullptr, bounds);
    return (advance > 0.0f) ? advance : fontSize * 0.6f;
}

static std::vector<std::string> SplitLines(const std::string& text)
{
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') {
            lines.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    lines.push_back(current);
    return lines;
}

void DrawFunctionEditorDialog(NVGcontext* vg, const FunctionEditorDialog& dialog,
                              float screenWidth, float screenHeight)
{
    if (!dialog.IsOpen()) {
        return;
    }

    // Modal dim overlay.
    nvgBeginPath(vg);
    nvgRect(vg, 0.0f, 0.0f, screenWidth, screenHeight);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 120));
    nvgFill(vg);

    const float x = dialog.GetX();
    const float y = dialog.GetY();
    const float width = FunctionEditorDialog::WIDTH;
    const float height = dialog.GetPanelHeight();

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 5.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(24, 24, 28, 250));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Title.
    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, 15.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(240, 240, 245));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + FunctionEditorDialog::PADDING,
            y + FunctionEditorDialog::PADDING + FunctionEditorDialog::TITLE_HEIGHT * 0.5f,
            "Wasm Function Editor", nullptr);

    // Name field.
    const UIRect nameRect = dialog.NameFieldRect();
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, EDITOR_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(170, 170, 178));
    nvgText(vg, x + FunctionEditorDialog::PADDING, nameRect.y + nameRect.h * 0.5f,
            "Name", nullptr);

    const bool nameFocused = dialog.GetFocus() == FunctionEditorDialog::Focus::Name;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, nameRect.x, nameRect.y, nameRect.w, nameRect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(15, 15, 17));
    nvgFill(vg);
    nvgStrokeColor(vg, nameFocused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);
    nvgFillColor(vg, nvgRGB(235, 235, 240));
    const std::string shownName = nameFocused ? dialog.GetFunctionName() + "|"
                                              : dialog.GetFunctionName();
    nvgText(vg, nameRect.x + 6.0f * UI_SCALE, nameRect.y + nameRect.h * 0.5f,
            shownName.c_str(), nullptr);

    // Source area.
    const UIRect sourceRect = dialog.SourceRect();
    const bool sourceFocused = dialog.GetFocus() == FunctionEditorDialog::Focus::Source;
    nvgBeginPath(vg);
    nvgRoundedRect(vg, sourceRect.x, sourceRect.y, sourceRect.w, sourceRect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(12, 12, 14));
    nvgFill(vg);
    nvgStrokeColor(vg, sourceFocused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    const std::vector<std::string> lines = SplitLines(dialog.GetSource());
    int caretLine = 0;
    int caretColumn = 0;
    dialog.GetCaretLineColumn(caretLine, caretColumn);
    const int firstLine = dialog.GetFirstVisibleLine();
    const float charWidth = dialog.GetCharWidth();
    const float textLeft = sourceRect.x + FunctionEditorDialog::TEXT_INSET;

    nvgSave(vg);
    nvgIntersectScissor(vg, sourceRect.x, sourceRect.y, sourceRect.w, sourceRect.h);

    // Selection highlight behind the text (monospace column math).
    if (dialog.HasSelection()) {
        std::size_t selectionBegin = 0;
        std::size_t selectionEnd = 0;
        dialog.GetSelectionRange(selectionBegin, selectionEnd);
        int beginLine = 0;
        int beginColumn = 0;
        int endLine = 0;
        int endColumn = 0;
        dialog.GetLineColumnAt(selectionBegin, beginLine, beginColumn);
        dialog.GetLineColumnAt(selectionEnd, endLine, endColumn);

        nvgFillColor(vg, nvgRGBA(70, 110, 180, 90));
        for (int line = beginLine; line <= endLine; ++line) {
            const int viewRow = line - firstLine;
            if (viewRow < 0 || viewRow >= FunctionEditorDialog::SOURCE_VISIBLE_LINES
                || line >= static_cast<int>(lines.size())) {
                continue;
            }
            const int lineLength =
                static_cast<int>(lines[static_cast<std::size_t>(line)].size());
            const int fromColumn = (line == beginLine) ? beginColumn : 0;
            int toColumn = (line == endLine) ? endColumn : lineLength;
            // Show newline selection as a small stub past the line end.
            if (toColumn == fromColumn) {
                toColumn = fromColumn + 1;
            }
            const float rowY = sourceRect.y
                             + FunctionEditorDialog::LINE_HEIGHT
                                   * static_cast<float>(viewRow);
            nvgBeginPath(vg);
            nvgRect(vg, textLeft + charWidth * static_cast<float>(fromColumn), rowY,
                    charWidth * static_cast<float>(toColumn - fromColumn),
                    FunctionEditorDialog::LINE_HEIGHT);
            nvgFill(vg);
        }
    }

    nvgFontFace(vg, FONT_MONO);
    nvgFontSize(vg, EDITOR_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(210, 215, 210));
    for (int i = 0; i < FunctionEditorDialog::SOURCE_VISIBLE_LINES; ++i) {
        const int lineIndex = firstLine + i;
        if (lineIndex < 0 || lineIndex >= static_cast<int>(lines.size())) {
            continue;
        }
        const float lineY = sourceRect.y
                          + FunctionEditorDialog::LINE_HEIGHT * (static_cast<float>(i) + 0.5f);
        nvgText(vg, textLeft, lineY,
                lines[static_cast<std::size_t>(lineIndex)].c_str(), nullptr);
    }

    // Caret.
    if (sourceFocused && caretLine >= firstLine
        && caretLine < firstLine + FunctionEditorDialog::SOURCE_VISIBLE_LINES) {
        const float caretX = textLeft + charWidth * static_cast<float>(caretColumn);
        const float caretTop = sourceRect.y
                             + FunctionEditorDialog::LINE_HEIGHT
                                   * static_cast<float>(caretLine - firstLine);
        nvgBeginPath(vg);
        nvgRect(vg, caretX, caretTop + 2.0f, 1.5f * UI_SCALE,
                FunctionEditorDialog::LINE_HEIGHT - 4.0f);
        nvgFillColor(vg, nvgRGB(140, 180, 240));
        nvgFill(vg);
    }
    nvgFontFace(vg, FONT_REGULAR);
    nvgRestore(vg);

    // Status line.
    const std::string& statusText = dialog.GetStatusText();
    if (!statusText.empty()) {
        nvgFillColor(vg, nvgRGB(230, 150, 90));
        nvgText(vg, x + FunctionEditorDialog::PADDING,
                sourceRect.y + sourceRect.h + FunctionEditorDialog::GAP
                    + FunctionEditorDialog::STATUS_HEIGHT * 0.5f,
                statusText.c_str(), nullptr);
    }

    // Buttons.
    const UIRect buildRect = dialog.BuildButtonRect();
    nvgBeginPath(vg);
    nvgRoundedRect(vg, buildRect.x, buildRect.y, buildRect.w, buildRect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(45, 110, 60));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGB(235, 240, 235));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, buildRect.x + buildRect.w * 0.5f, buildRect.y + buildRect.h * 0.5f,
            "Build & Save", nullptr);

    const UIRect cancelRect = dialog.CancelButtonRect();
    nvgBeginPath(vg);
    nvgRoundedRect(vg, cancelRect.x, cancelRect.y, cancelRect.w, cancelRect.h, 3.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGB(45, 45, 50));
    nvgFill(vg);
    nvgFillColor(vg, nvgRGB(230, 230, 235));
    nvgText(vg, cancelRect.x + cancelRect.w * 0.5f, cancelRect.y + cancelRect.h * 0.5f,
            "Cancel", nullptr);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
}
