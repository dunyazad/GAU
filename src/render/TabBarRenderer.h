#pragma once

#include <string>
#include <vector>

struct NVGcontext;

// Draws the document tab bar (tabs, "+", Open/Save/Save As) across the
// top of the window in screen space. Stateless; layout comes from
// interaction/TabBar.h.
void DrawTabBar(NVGcontext* vg, const std::vector<std::string>& tabNames,
                int activeTabIndex, float screenWidth);
