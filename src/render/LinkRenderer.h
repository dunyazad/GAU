#pragma once

#include "model/GraphTypes.h"

struct NVGcontext;
class NodeGraph;
class NodeLayoutCache;

// Draws all links as cubic beziers with horizontal tangents (design
// spec: exec links white 3px, data links pin-colored 2px). Assumes the
// canvas transform is applied and the layout cache holds this frame's
// pin positions. Stateless.
void DrawLinks(NVGcontext* vg, const NodeGraph& graph, const NodeLayoutCache& layoutCache);

// Draws the in-progress link while dragging from a pin to the current
// mouse position (canvas coordinates).
void DrawDraggingLink(NVGcontext* vg, const NodeLayoutCache& layoutCache,
                      PinId fromPinId, float toCanvasX, float toCanvasY);
