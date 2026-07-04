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

} // namespace gau::render
