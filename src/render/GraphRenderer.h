#pragma once

// Draws a v2 Graph with NanoVG using a precomputed GraphLayout and the
// canvas transform. Build-verified (needs a live NVGcontext).

#include "GraphLayout.h"
#include "RenderCanvas.h"

#include "model/Graph.h"

struct NVGcontext;

namespace gau {
class TypeRegistry;
}

namespace gau::render {

void DrawGraph(NVGcontext* vg, const Canvas& canvas, const Graph& graph,
               const TypeRegistry& types, const GraphLayout& layout);

// Overlay: selection outlines and the in-progress link wire. Drawn after
// DrawGraph with the same canvas transform.
void DrawSelection(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout,
                   const std::vector<NodeId>& selection);
void DrawDragLink(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout, PinId fromPin,
                  float dragCanvasX, float dragCanvasY);

// Colored outline around a single node (breakpoints, debug cursor).
void DrawNodeOutline(NVGcontext* vg, const Canvas& canvas, const GraphLayout& layout, NodeId node,
                     int r, int g, int b, float width);

// The in-progress rubber-band selection rectangle (canvas coordinates,
// any corner order).
void DrawRubberBand(NVGcontext* vg, const Canvas& canvas, float x0, float y0, float x1, float y1);

} // namespace gau::render
