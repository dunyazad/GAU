#include "model/ExecPinPolicy.h"

#include <cstdio>
#include <vector>

static int failCount = 0;

static void Check(bool condition, const char* label)
{
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failCount;
    }
}

static PinDef Pin(PinDirection direction, PinType type, const char* name)
{
    PinDef pin;
    pin.direction = direction;
    pin.type = type;
    pin.name = name;
    return pin;
}

static int CountExec(const std::vector<PinDef>& pins, PinDirection direction)
{
    int count = 0;
    for (const PinDef& pin : pins) {
        if (pin.direction == direction && pin.type == PinType::Exec) {
            ++count;
        }
    }
    return count;
}

// A Function class authored with data pins only gains one exec input
// ahead of the data inputs and a default "then" exec output ahead of
// the data outputs.
static void TestFunctionGainsExecPins()
{
    std::vector<PinDef> pins = NormalizeClassExecPins(
        "Function", {Pin(PinDirection::Input, PinType::Float, "x"),
                     Pin(PinDirection::Output, PinType::String, "result")});
    Check(pins.size() == 4, "exec in + x + then + result");
    if (pins.size() == 4) {
        Check(pins[0].type == PinType::Exec && pins[0].direction == PinDirection::Input,
              "exec input comes first");
        Check(pins[1].name == "x", "data input follows the exec input");
        Check(pins[2].type == PinType::Exec && pins[2].direction == PinDirection::Output
                  && pins[2].name == "then",
              "default exec output precedes the data outputs");
        Check(pins[3].name == "result", "data output last");
    }
}

// Pure and Object classes never carry exec pins, even when authored.
static void TestPureStripsExecPins()
{
    std::vector<PinDef> pins = NormalizeClassExecPins(
        "Pure", {Pin(PinDirection::Input, PinType::Exec, ""),
                 Pin(PinDirection::Input, PinType::Int, "a"),
                 Pin(PinDirection::Output, PinType::Exec, "then"),
                 Pin(PinDirection::Output, PinType::Int, "r")});
    Check(pins.size() == 2, "only data pins survive on a Pure class");
    Check(CountExec(pins, PinDirection::Input) == 0
              && CountExec(pins, PinDirection::Output) == 0,
          "no exec pins on a Pure class");
}

// Event classes source the flow: no exec input, one auto exec output.
static void TestEventHasOutputOnly()
{
    std::vector<PinDef> pins = NormalizeClassExecPins("Event", {});
    Check(pins.size() == 1 && pins[0].type == PinType::Exec
              && pins[0].direction == PinDirection::Output,
          "event gets a single exec output");
    Check(CountExec(pins, PinDirection::Input) == 0, "event has no exec input");
}

// Branch-style authored exec outputs are kept in order (no extra
// "then"), authored exec inputs collapse to the single automatic one,
// and exec pins sort ahead of data pins.
static void TestAuthoredExecOutputsKept()
{
    std::vector<PinDef> pins = NormalizeClassExecPins(
        "FlowControl", {Pin(PinDirection::Input, PinType::Bool, "condition"),
                        Pin(PinDirection::Input, PinType::Exec, ""),
                        Pin(PinDirection::Input, PinType::Exec, "extra"),
                        Pin(PinDirection::Output, PinType::Int, "count"),
                        Pin(PinDirection::Output, PinType::Exec, "True"),
                        Pin(PinDirection::Output, PinType::Exec, "False")});
    Check(pins.size() == 5, "1 exec in + condition + True/False + count");
    if (pins.size() == 5) {
        Check(pins[0].type == PinType::Exec && pins[0].direction == PinDirection::Input,
              "single exec input first");
        Check(CountExec(pins, PinDirection::Input) == 1,
              "authored exec inputs collapse to one");
        Check(pins[1].name == "condition", "data input follows the exec input");
        Check(pins[2].name == "True" && pins[3].name == "False",
              "authored exec outputs keep their order ahead of data outputs");
        Check(pins[4].name == "count", "data output last");
        Check(CountExec(pins, PinDirection::Output) == 2, "no extra then added");
    }
}

int main()
{
    TestFunctionGainsExecPins();
    TestPureStripsExecPins();
    TestEventHasOutputOnly();
    TestAuthoredExecOutputsKept();
    if (failCount == 0) {
        std::printf("exec_pin_policy_tests: all passed\n");
    }
    return failCount == 0 ? 0 : 1;
}
