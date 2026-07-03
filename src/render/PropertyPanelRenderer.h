#pragma once

struct NVGcontext;
struct Node;
class PropertyPanel;

// Draws the dockable property inspector for the selected node in screen
// space. Stateless; reads the panel state read-only and shares its rect
// getters for layout.
void DrawPropertyPanel(NVGcontext* vg, const PropertyPanel& panel, const Node& node,
                       float screenWidth);
