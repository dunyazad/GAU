#include "NodeClass.h"

// Builtin node type definitions (the M5 set). Each static instance
// self-registers with NodeClass::GetRegistry(); definition order here
// is the display order in the creation menu.

static const NodeClass EventBeginClass("Event Begin", "Event", {
    {PinDirection::Output, PinType::Exec, ""},
});

static const NodeClass BranchClass("Branch", "FlowControl", {
    {PinDirection::Input, PinType::Exec, ""},
    {PinDirection::Input, PinType::Bool, "Condition"},
    {PinDirection::Output, PinType::Exec, "True"},
    {PinDirection::Output, PinType::Exec, "False"},
});

static const NodeClass SequenceClass("Sequence", "FlowControl", {
    {PinDirection::Input, PinType::Exec, ""},
    {PinDirection::Output, PinType::Exec, "Then 0"},
    {PinDirection::Output, PinType::Exec, "Then 1"},
});

static const NodeClass PrintStringClass("Print String", "Function", {
    {PinDirection::Input, PinType::Exec, ""},
    {PinDirection::Input, PinType::String, "In String"},
    {PinDirection::Output, PinType::Exec, ""},
});

static const NodeClass MakeIntClass("Make Int", "Pure", {
    {PinDirection::Output, PinType::Int, "Value"},
});

static const NodeClass MakeFloatClass("Make Float", "Pure", {
    {PinDirection::Output, PinType::Float, "Value"},
});

static const NodeClass AddClass("Add", "Pure", {
    {PinDirection::Input, PinType::Int, "A"},
    {PinDirection::Input, PinType::Int, "B"},
    {PinDirection::Output, PinType::Int, "Result"},
});

static const NodeClass CompareClass("Compare", "Pure", {
    {PinDirection::Input, PinType::Int, "A"},
    {PinDirection::Input, PinType::Int, "B"},
    {PinDirection::Output, PinType::Bool, "Result"},
});

static const NodeClass ForLoopClass("For Loop", "FlowControl", {
    {PinDirection::Input, PinType::Exec, ""},
    {PinDirection::Input, PinType::Int, "First Index"},
    {PinDirection::Input, PinType::Int, "Last Index"},
    {PinDirection::Output, PinType::Exec, "Loop Body"},
    {PinDirection::Output, PinType::Int, "Index"},
    {PinDirection::Output, PinType::Exec, "Completed"},
});
