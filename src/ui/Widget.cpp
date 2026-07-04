// Retained-mode widget implementations.

#include "Widget.h"

#include <algorithm>

namespace gau::ui {

static const float BUTTON_PAD_X = 10.0f;
static const float BUTTON_PAD_Y = 6.0f;
static const float LABEL_PAD = 2.0f;

Widget* Widget::Add(std::unique_ptr<Widget> child)
{
    Widget* raw = child.get();
    children.push_back(std::move(child));
    return raw;
}

Size Widget::Measure(Painter& painter, Size available)
{
    Size result{};
    for (const std::unique_ptr<Widget>& child : children) {
        const Size childSize = child->Measure(painter, available);
        result.w = std::max(result.w, childSize.w);
        result.h = std::max(result.h, childSize.h);
    }
    measured = result;
    return result;
}

void Widget::Arrange(const Rect& finalRect)
{
    bounds = finalRect;
    for (const std::unique_ptr<Widget>& child : children) {
        child->Arrange(finalRect);
    }
}

void Widget::Paint(Painter& painter)
{
    for (const std::unique_ptr<Widget>& child : children) {
        child->Paint(painter);
    }
}

bool Widget::OnEvent(const Event& event)
{
    for (auto it = children.rbegin(); it != children.rend(); ++it) {
        Widget* child = it->get();
        if (event.IsPositional() && !child->Bounds().Contains(event.x, event.y)) {
            continue;
        }
        if (child->OnEvent(event)) {
            return true;
        }
    }
    return false;
}

Size Column::Measure(Painter& painter, Size available)
{
    Size result{};
    for (std::size_t i = 0; i < children.size(); ++i) {
        const Size childSize = children[i]->Measure(painter, available);
        result.w = std::max(result.w, childSize.w);
        result.h += childSize.h;
        if (i + 1 < children.size()) {
            result.h += spacing;
        }
    }
    measured = result;
    return result;
}

void Column::Arrange(const Rect& finalRect)
{
    bounds = finalRect;
    float y = finalRect.y;
    for (const std::unique_ptr<Widget>& child : children) {
        const float h = child->Measured().h;
        child->Arrange(Rect{finalRect.x, y, finalRect.w, h});
        y += h + spacing;
    }
}

Size Row::Measure(Painter& painter, Size available)
{
    Size result{};
    for (std::size_t i = 0; i < children.size(); ++i) {
        const Size childSize = children[i]->Measure(painter, available);
        result.h = std::max(result.h, childSize.h);
        result.w += childSize.w;
        if (i + 1 < children.size()) {
            result.w += spacing;
        }
    }
    measured = result;
    return result;
}

void Row::Arrange(const Rect& finalRect)
{
    bounds = finalRect;
    float x = finalRect.x;
    for (const std::unique_ptr<Widget>& child : children) {
        const float w = child->Measured().w;
        child->Arrange(Rect{x, finalRect.y, w, finalRect.h});
        x += w + spacing;
    }
}

void Panel::Paint(Painter& painter)
{
    painter.FillRect(bounds, background);
    Widget::Paint(painter);
}

Size Label::Measure(Painter& painter, Size available)
{
    (void)available;
    const float w = painter.MeasureText(text, fontSize) + LABEL_PAD * 2.0f;
    measured = Size{w, fontSize + LABEL_PAD * 2.0f};
    return measured;
}

void Label::Paint(Painter& painter)
{
    painter.Text(bounds.x + LABEL_PAD, bounds.y + bounds.h * 0.5f, text,
                 Color{225, 225, 230, 255}, fontSize);
}

Size Button::Measure(Painter& painter, Size available)
{
    (void)available;
    const float w = painter.MeasureText(text, fontSize) + BUTTON_PAD_X * 2.0f;
    measured = Size{w, fontSize + BUTTON_PAD_Y * 2.0f};
    return measured;
}

void Button::Paint(Painter& painter)
{
    const Color fill = hovered ? Color{70, 110, 180, 255} : Color{45, 45, 50, 255};
    painter.FillRect(bounds, fill);
    painter.StrokeRect(bounds, Color{70, 70, 78, 255}, 1.0f);
    painter.Text(bounds.x + BUTTON_PAD_X, bounds.y + bounds.h * 0.5f, text,
                 Color{230, 230, 235, 255}, fontSize);
}

bool Button::OnEvent(const Event& event)
{
    if (event.type == EventType::MouseMove) {
        hovered = bounds.Contains(event.x, event.y);
        return false;
    }
    if (event.type == EventType::MouseDown && event.button == 0
        && bounds.Contains(event.x, event.y)) {
        if (onClick) {
            onClick();
        }
        return true;
    }
    return false;
}

static const float FIELD_PAD_X = 6.0f;
static const float FIELD_PAD_Y = 5.0f;

Size TextField::Measure(Painter& painter, Size available)
{
    (void)available;
    const float textW = painter.MeasureText(value, fontSize) + FIELD_PAD_X * 2.0f;
    measured = Size{std::max(minWidth, textW), fontSize + FIELD_PAD_Y * 2.0f};
    return measured;
}

void TextField::Paint(Painter& painter)
{
    const Color bg = focused ? Color{40, 40, 48, 255} : Color{30, 30, 36, 255};
    painter.FillRect(bounds, bg);
    painter.StrokeRect(bounds, focused ? Color{255, 180, 40, 255} : Color{70, 70, 78, 255},
                       focused ? 2.0f : 1.0f);
    const std::string shown = focused ? value + "|" : value;
    painter.Text(bounds.x + FIELD_PAD_X, bounds.y + bounds.h * 0.5f, shown,
                 Color{230, 230, 235, 255}, fontSize);
}

bool TextField::OnEvent(const Event& event)
{
    if (event.type == EventType::MouseDown) {
        focused = bounds.Contains(event.x, event.y);
        return focused;
    }
    if (!focused) {
        return false;
    }
    if (event.type == EventType::Text) {
        value += event.text; // event.text is null-terminated UTF-8
        if (onChange) {
            onChange(value);
        }
        return true;
    }
    if (event.type == EventType::Key) {
        if (event.key == keys::Backspace) {
            // Remove one UTF-8 code point from the end.
            while (!value.empty()) {
                const unsigned char c = static_cast<unsigned char>(value.back());
                value.pop_back();
                if ((c & 0xC0) != 0x80) {
                    break;
                }
            }
            if (onChange) {
                onChange(value);
            }
            return true;
        }
        if (event.key == keys::Enter || event.key == keys::Escape) {
            focused = false;
            return true;
        }
    }
    return false;
}

} // namespace gau::ui
