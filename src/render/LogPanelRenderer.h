#pragma once

#include <string>
#include <vector>

struct NVGcontext;
class LogPanel;

// Draws the bottom log strip. Stateless; layout comes from the panel.
void DrawLogPanel(NVGcontext* vg, const LogPanel& panel,
                  const std::vector<std::string>& lines,
                  float screenWidth, float screenHeight);
