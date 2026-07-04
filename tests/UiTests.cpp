#include "ui/Widget.h"

#include <cmath>
#include <cstdio>
#include <memory>

using namespace gau::ui;

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static void CheckNear(float actual, float expected, const char* label)
{
    if (std::fabs(actual - expected) > 0.01f) {
        std::printf("FAIL: %s: expected %.3f, got %.3f\n", label, expected, actual);
        ++failCount;
    }
}

// Deterministic painter for tests: monospace-ish text metrics, records
// how many primitives were drawn.
class RecordingPainter : public Painter
{
public:
    int fills = 0;
    int strokes = 0;
    int texts = 0;

    void FillRect(const Rect&, Color) override { ++fills; }
    void StrokeRect(const Rect&, Color, float) override { ++strokes; }
    void Text(float, float, const std::string&, Color, float) override { ++texts; }
    float MeasureText(const std::string& text, float size) override
    {
        return static_cast<float>(text.size()) * size * 0.5f;
    }
};

static void TestLayoutAndEvents()
{
    bool clickedA = false;
    bool clickedB = false;

    auto panel = std::make_unique<Panel>(Color{22, 22, 25, 255});
    auto column = std::make_unique<Column>(4.0f);
    Widget* colPtr = column.get();
    colPtr->Add(std::make_unique<Button>("A", [&clickedA]() { clickedA = true; }));
    colPtr->Add(std::make_unique<Button>("B", [&clickedB]() { clickedB = true; }));
    panel->Add(std::move(column));

    RecordingPainter painter;
    const Size measured = panel->Measure(painter, Size{200.0f, 200.0f});
    panel->Arrange(Rect{0.0f, 0.0f, 200.0f, measured.h});

    // Each button: fontSize 14 + padY 12 = 26 high; spacing 4.
    const Widget* buttonA = colPtr->Children()[0].get();
    const Widget* buttonB = colPtr->Children()[1].get();
    CheckNear(buttonA->Bounds().y, 0.0f, "button A at top");
    CheckNear(buttonA->Bounds().h, 26.0f, "button height");
    CheckNear(buttonB->Bounds().y, 30.0f, "button B below A + spacing");
    CheckNear(buttonB->Bounds().w, 200.0f, "button fills column width");

    // Click inside button B.
    Event down;
    down.type = EventType::MouseDown;
    down.x = 100.0f;
    down.y = 40.0f;
    const bool handled = panel->OnEvent(down);
    Check(handled, "mouse down handled");
    Check(clickedB && !clickedA, "only button B fired");

    // Paint records panel background + 2 button fills + strokes + texts.
    RecordingPainter rec;
    panel->Paint(rec);
    Check(rec.fills >= 3, "panel + 2 buttons filled");
    Check(rec.texts >= 2, "2 button labels drawn");
}

int main()
{
    TestLayoutAndEvents();
    if (failCount == 0) {
        std::printf("ui_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
