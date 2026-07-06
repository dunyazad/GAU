// NanoVG-backed Painter.

#include "NanoVgPainter.h"

#include <nanovg.h>

namespace gau::render {

static NVGcolor ToNvg(gau::ui::Color c)
{
    return nvgRGBA(c.r, c.g, c.b, c.a);
}

void NanoVgPainter::FillRect(const gau::ui::Rect& rect, gau::ui::Color color)
{
    nvgBeginPath(vg);
    nvgRect(vg, rect.x, rect.y, rect.w, rect.h);
    nvgFillColor(vg, ToNvg(color));
    nvgFill(vg);
}

void NanoVgPainter::StrokeRect(const gau::ui::Rect& rect, gau::ui::Color color, float width)
{
    nvgBeginPath(vg);
    nvgRect(vg, rect.x, rect.y, rect.w, rect.h);
    nvgStrokeColor(vg, ToNvg(color));
    nvgStrokeWidth(vg, width);
    nvgStroke(vg);
}

void NanoVgPainter::Text(float x, float y, const std::string& text, gau::ui::Color color,
                         float size)
{
    nvgFontFace(vg, font);
    nvgFontSize(vg, size);
    nvgFillColor(vg, ToNvg(color));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x, y, text.c_str(), nullptr);
}

float NanoVgPainter::MeasureText(const std::string& text, float size)
{
    // Key by size + text; sizes are few and never collide with text
    // content thanks to the separator.
    std::string key = std::to_string(size);
    key += '|';
    key += text;
    const auto cached = measureCache.find(key);
    if (cached != measureCache.end()) {
        return cached->second;
    }

    nvgFontFace(vg, font);
    nvgFontSize(vg, size);
    float bounds[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    nvgTextBounds(vg, 0.0f, 0.0f, text.c_str(), nullptr, bounds);
    const float width = bounds[2] - bounds[0];
    measureCache.emplace(std::move(key), width);
    return width;
}

} // namespace gau::render
