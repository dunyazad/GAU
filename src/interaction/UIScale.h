#pragma once

// Global scale for screen-space UI (context menu, dialogs): fonts and
// layout boxes multiply by this. Canvas content (nodes, grid) is not
// affected; it scales with the canvas zoom instead (see
// DEFAULT_CANVAS_ZOOM in main.cpp).
constexpr float UI_SCALE = 2.0f;
