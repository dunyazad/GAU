#pragma once

// NanoVG-backed implementation of the UI toolkit's Painter. Requires a
// live NVGcontext (GL/Metal), so it is build-verified rather than unit
// tested.

#include "ui/Painter.h"

#include <string>
#include <unordered_map>

struct NVGcontext;

namespace gau::render {

class NanoVgPainter : public gau::ui::Painter
{
public:
    NanoVgPainter(NVGcontext* vg, const char* font) : vg(vg), font(font) {}

    void FillRect(const gau::ui::Rect& rect, gau::ui::Color color) override;
    void StrokeRect(const gau::ui::Rect& rect, gau::ui::Color color, float width) override;
    void Text(float x, float y, const std::string& text, gau::ui::Color color,
              float size) override;
    float MeasureText(const std::string& text, float size) override;

private:
    NVGcontext* vg;
    const char* font;
    // Text measurement memo: the graph layout re-measures every label
    // every frame (NFR-1), and widths never change for a given
    // text/size pair within one font.
    std::unordered_map<std::string, float> measureCache;
};

} // namespace gau::render
