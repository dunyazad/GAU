#pragma once

struct NVGcontext;
class ConfirmSaveDialog;

// Draws the in-app confirm-save modal: a dimmed backdrop plus a themed
// panel with title, message and Save / Don't Save / Cancel buttons.
void DrawConfirmSaveDialog(NVGcontext* vg, const ConfirmSaveDialog& dialog, float screenWidth,
                           float screenHeight);
