#pragma once

// Abstract drawing surface for widgets. The NanoVG implementation lives in
// the render layer (needs a GL/Metal context); tests use a recording or
// no-op painter, so the UI toolkit itself needs no graphics backend.

#include "Geometry.h"

#include <string>

namespace gau::ui {

class Painter
{
public:
    virtual ~Painter() = default;

    virtual void FillRect(const Rect& rect, Color color) = 0;
    virtual void StrokeRect(const Rect& rect, Color color, float width) = 0;
    virtual void Text(float x, float y, const std::string& text, Color color, float size) = 0;
    // Advance width of text at a given size (for widget measurement).
    virtual float MeasureText(const std::string& text, float size) = 0;
};

} // namespace gau::ui
