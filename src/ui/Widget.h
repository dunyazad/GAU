#pragma once

// Retained-mode widget tree: Measure (desired size) -> Arrange (final
// rect) -> Paint, with event dispatch by hit test. Containers position
// their children; leaf widgets draw and handle input.

#include "Geometry.h"
#include "Painter.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace gau::ui {

class Widget
{
public:
    virtual ~Widget() = default;

    // Adds a child and returns a non-owning pointer to it.
    Widget* Add(std::unique_ptr<Widget> child);

    virtual Size Measure(Painter& painter, Size available);
    virtual void Arrange(const Rect& finalRect);
    virtual void Paint(Painter& painter);
    // Returns true when consumed. Positional events are hit-tested against
    // children; others fan out until handled.
    virtual bool OnEvent(const Event& event);

    const Rect& Bounds() const { return bounds; }
    // Size from the last Measure pass; used by containers during Arrange.
    const Size& Measured() const { return measured; }
    const std::vector<std::unique_ptr<Widget>>& Children() const { return children; }

protected:
    Rect bounds{};
    Size measured{};
    std::vector<std::unique_ptr<Widget>> children;
};

// Vertical stack.
class Column : public Widget
{
public:
    explicit Column(float spacing = 4.0f) : spacing(spacing) {}
    Size Measure(Painter& painter, Size available) override;
    void Arrange(const Rect& finalRect) override;

private:
    float spacing;
};

// Horizontal stack.
class Row : public Widget
{
public:
    explicit Row(float spacing = 4.0f) : spacing(spacing) {}
    Size Measure(Painter& painter, Size available) override;
    void Arrange(const Rect& finalRect) override;

private:
    float spacing;
};

// Background rectangle wrapping its children (overlay layout).
class Panel : public Widget
{
public:
    explicit Panel(Color background) : background(background) {}
    void Paint(Painter& painter) override;

private:
    Color background;
};

class Label : public Widget
{
public:
    Label(std::string text, float fontSize = 14.0f)
        : text(std::move(text)), fontSize(fontSize) {}
    Size Measure(Painter& painter, Size available) override;
    void Paint(Painter& painter) override;
    void SetText(std::string value) { text = std::move(value); }

private:
    std::string text;
    float fontSize;
};

class Button : public Widget
{
public:
    Button(std::string text, std::function<void()> onClick, float fontSize = 14.0f)
        : text(std::move(text)), onClick(std::move(onClick)), fontSize(fontSize) {}
    Size Measure(Painter& painter, Size available) override;
    void Paint(Painter& painter) override;
    bool OnEvent(const Event& event) override;

private:
    std::string text;
    std::function<void()> onClick;
    float fontSize;
    bool hovered = false;
};

} // namespace gau::ui
