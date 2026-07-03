#include "ClassEditorDialogRenderer.h"

#include "interaction/ClassEditorDialog.h"

#include <nanovg.h>

#include <string>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const float DIALOG_FONT_SIZE = 13.0f;

static const char* CategoryDisplayName(NodeCategory category)
{
    switch (category) {
    case NodeCategory::Event:
        return "Event";
    case NodeCategory::Function:
        return "Function";
    case NodeCategory::FlowControl:
        return "Flow Control";
    case NodeCategory::Pure:
        return "Pure";
    }
    return "";
}

static const char* PinTypeDisplayName(PinType type)
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
    }
    return "";
}

static void DrawButton(NVGcontext* vg, const UIRect& rect, const char* label, bool accent)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f);
    nvgFillColor(vg, accent ? nvgRGB(50, 90, 160) : nvgRGB(45, 45, 50));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(230, 230, 235));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, nullptr);
}

static void DrawTextField(NVGcontext* vg, const UIRect& rect, const std::string& text,
                          const char* placeholder, bool focused)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f);
    nvgFillColor(vg, nvgRGB(15, 15, 17));
    nvgFill(vg);
    nvgStrokeColor(vg, focused ? nvgRGB(70, 110, 180) : nvgRGB(60, 60, 66));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);

    nvgSave(vg);
    nvgIntersectScissor(vg, rect.x, rect.y, rect.w, rect.h);
    const float textX = rect.x + 7.0f;
    const float textY = rect.y + rect.h * 0.5f;
    if (text.empty() && !focused) {
        nvgFillColor(vg, nvgRGB(110, 110, 118));
        nvgText(vg, textX, textY, placeholder, nullptr);
    } else {
        nvgFillColor(vg, nvgRGB(235, 235, 240));
        const std::string shown = focused ? text + "|" : text;
        nvgText(vg, textX, textY, shown.c_str(), nullptr);
    }
    nvgRestore(vg);
}

static void DrawRowLabel(NVGcontext* vg, float x, float centerY, const char* label)
{
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, DIALOG_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(170, 170, 178));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x, centerY, label, nullptr);
}

static void DrawPinRows(NVGcontext* vg, const ClassEditorDialog& dialog)
{
    const std::vector<PinDef>& pins = dialog.GetPins();
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        const PinDef& pin = pins[static_cast<std::size_t>(i)];

        DrawButton(vg, dialog.PinDirectionRect(i),
                   pin.direction == PinDirection::Input ? "In" : "Out", false);
        DrawButton(vg, dialog.PinTypeRect(i), PinTypeDisplayName(pin.type), false);

        const bool focused = dialog.GetFocus() == ClassEditorDialog::Focus::PinName
                          && dialog.GetFocusedPinIndex() == i;
        DrawTextField(vg, dialog.PinNameRect(i), pin.name, "Pin name", focused);

        DrawButton(vg, dialog.PinRemoveRect(i), "x", false);
    }
}

void DrawClassEditorDialog(NVGcontext* vg, const ClassEditorDialog& dialog,
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
    const float width = ClassEditorDialog::WIDTH;
    const float height = dialog.GetPanelHeight();

    // Panel.
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, width, height, 5.0f);
    nvgFillColor(vg, nvgRGBA(24, 24, 28, 250));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Title.
    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, 15.0f);
    nvgFillColor(vg, nvgRGB(240, 240, 245));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + ClassEditorDialog::PADDING,
            y + ClassEditorDialog::PADDING + ClassEditorDialog::TITLE_HEIGHT * 0.5f,
            "Create Node Class", nullptr);

    // Name row.
    const UIRect nameRect = dialog.NameFieldRect();
    DrawRowLabel(vg, x + ClassEditorDialog::PADDING, nameRect.y + nameRect.h * 0.5f, "Name");
    DrawTextField(vg, nameRect, dialog.GetClassNameText(), "Class name",
                  dialog.GetFocus() == ClassEditorDialog::Focus::ClassName);

    // Category row.
    const UIRect categoryRect = dialog.CategoryButtonRect();
    DrawRowLabel(vg, x + ClassEditorDialog::PADDING, categoryRect.y + categoryRect.h * 0.5f, "Category");
    DrawButton(vg, categoryRect, CategoryDisplayName(dialog.GetCategory()), false);

    // Pins label + rows.
    const float pinsLabelY = categoryRect.y + ClassEditorDialog::ROW_HEIGHT
                           + ClassEditorDialog::GAP + ClassEditorDialog::PINS_LABEL_HEIGHT * 0.5f;
    DrawRowLabel(vg, x + ClassEditorDialog::PADDING, pinsLabelY, "Pins");
    DrawPinRows(vg, dialog);

    DrawButton(vg, dialog.AddPinButtonRect(), "+ Add Pin", false);

    // Error line.
    const std::string& errorText = dialog.GetErrorText();
    if (!errorText.empty()) {
        nvgFontFace(vg, FONT_REGULAR);
        nvgFontSize(vg, DIALOG_FONT_SIZE);
        nvgFillColor(vg, nvgRGB(230, 90, 90));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + ClassEditorDialog::PADDING,
                dialog.AddPinButtonRect().y + ClassEditorDialog::BUTTON_HEIGHT
                    + ClassEditorDialog::GAP + ClassEditorDialog::ERROR_HEIGHT * 0.5f,
                errorText.c_str(), nullptr);
    }

    DrawButton(vg, dialog.OkButtonRect(), "Create", true);
    DrawButton(vg, dialog.CancelButtonRect(), "Cancel", false);
}
