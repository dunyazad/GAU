#pragma once

struct NVGcontext;
class ContextMenu;

// Draws the node-creation context menu in screen space. Stateless;
// reads the menu state (read-only) from the interaction layer.
void DrawContextMenu(NVGcontext* vg, const ContextMenu& menu);
