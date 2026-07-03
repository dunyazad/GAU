#pragma once

struct NVGcontext;
class ClassEditorDialog;

// Draws the custom class editor dialog in screen space (with a modal
// dim overlay). Stateless; reads the dialog state read-only from the
// interaction layer and shares its rect getters for layout.
void DrawClassEditorDialog(NVGcontext* vg, const ClassEditorDialog& dialog,
                           float screenWidth, float screenHeight);
