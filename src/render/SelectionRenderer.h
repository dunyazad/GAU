#pragma once

struct NVGcontext;
class Canvas;

// Draws the rubber-band selection rectangle. Corners are canvas-space
// (any order); converted to screen space internally. Stateless.
void DrawRubberBand(NVGcontext* vg, const Canvas& canvas,
                    float canvasX0, float canvasY0, float canvasX1, float canvasY1);
