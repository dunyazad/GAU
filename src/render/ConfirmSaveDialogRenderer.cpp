#include "ConfirmSaveDialogRenderer.h"

#include "interaction/ConfirmSaveDialog.h"

#include <nanovg.h>

#include <string>

static const char* FONT_REGULAR = "sans";
static const char* FONT_BOLD = "sans-bold";
static const float MESSAGE_FONT_SIZE = 13.0f * UI_SCALE;
static const float TITLE_FONT_SIZE = 15.0f * UI_SCALE;

static void DrawButton(NVGcontext* vg, const UIRect& rect, const char* label, bool accent,
                       bool hovered)
{
    nvgBeginPath(vg);
    nvgRoundedRect(vg, rect.x, rect.y, rect.w, rect.h, 3.0f * UI_SCALE);
    NVGcolor fill = accent ? nvgRGB(50, 90, 160) : nvgRGB(45, 45, 50);
    if (hovered) {
        fill = accent ? nvgRGB(65, 110, 190) : nvgRGB(58, 58, 64);
    }
    nvgFillColor(vg, fill);
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MESSAGE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(230, 230, 235));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, rect.x + rect.w * 0.5f, rect.y + rect.h * 0.5f, label, nullptr);
}

void DrawConfirmSaveDialog(NVGcontext* vg, const ConfirmSaveDialog& dialog, float screenWidth,
                           float screenHeight)
{
    if (!dialog.IsOpen()) {
        return;
    }

    // Dim the editor behind the modal.
    nvgBeginPath(vg);
    nvgRect(vg, 0.0f, 0.0f, screenWidth, screenHeight);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 110));
    nvgFill(vg);

    const UIRect panel = dialog.PanelRect();
    nvgBeginPath(vg);
    nvgRoundedRect(vg, panel.x, panel.y, panel.w, panel.h, 6.0f * UI_SCALE);
    nvgFillColor(vg, nvgRGBA(28, 28, 32, 250));
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGB(70, 70, 78));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    nvgFontFace(vg, FONT_BOLD);
    nvgFontSize(vg, TITLE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(235, 235, 240));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, panel.x + ConfirmSaveDialog::PADDING,
            panel.y + ConfirmSaveDialog::PADDING + ConfirmSaveDialog::TITLE_HEIGHT * 0.5f,
            "Unsaved Changes", nullptr);

    const std::string message =
        "Save changes to \"" + dialog.GetDocumentName() + "\" before closing?";
    nvgFontFace(vg, FONT_REGULAR);
    nvgFontSize(vg, MESSAGE_FONT_SIZE);
    nvgFillColor(vg, nvgRGB(200, 200, 208));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextBox(vg, panel.x + ConfirmSaveDialog::PADDING,
               panel.y + ConfirmSaveDialog::PADDING + ConfirmSaveDialog::TITLE_HEIGHT
                   + 6.0f * UI_SCALE,
               panel.w - ConfirmSaveDialog::PADDING * 2.0f, message.c_str(), nullptr);

    const int hovered = dialog.GetHoveredButton();
    DrawButton(vg, dialog.SaveButtonRect(), "Save", true, hovered == 0);
    DrawButton(vg, dialog.DiscardButtonRect(), "Don't Save", false, hovered == 1);
    DrawButton(vg, dialog.CancelButtonRect(), "Cancel", false, hovered == 2);
}
