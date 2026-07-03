#pragma once

struct NVGcontext;
class FunctionEditorDialog;

// Draws the wasm function editor dialog (modal). Stateless.
void DrawFunctionEditorDialog(NVGcontext* vg, const FunctionEditorDialog& dialog,
                              float screenWidth, float screenHeight);

// Advance width of one glyph of the "mono" face at the given size;
// measured once at startup and handed to the dialog for caret math.
float MeasureMonoCharWidth(NVGcontext* vg, float fontSize);
