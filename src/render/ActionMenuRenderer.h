#pragma once

struct NVGcontext;
class ActionMenu;

// Draws the right-click action popup in screen space. Stateless.
void DrawActionMenu(NVGcontext* vg, const ActionMenu& menu);
