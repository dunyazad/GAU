#pragma once

struct NVGcontext;
class Canvas;

// Draws the two-level background grid (16px minor, 128px major at zoom 1)
// for the visible screen area. Stateless.
void DrawGrid(NVGcontext* vg, const Canvas& canvas, float screenWidth, float screenHeight);
