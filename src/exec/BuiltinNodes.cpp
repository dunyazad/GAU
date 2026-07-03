#include "BuiltinNodes.h"

#include <cstring>
#include <string>

// Tolerant coercions between Value alternatives.
static bool AsBool(const Value& value)
{
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return *boolValue;
    }
    if (const int* intValue = std::get_if<int>(&value)) {
        return *intValue != 0;
    }
    if (const double* doubleValue = std::get_if<double>(&value)) {
        return *doubleValue != 0.0;
    }
    if (const std::string* stringValue = std::get_if<std::string>(&value)) {
        return !stringValue->empty();
    }
    return false;
}

static int AsInt(const Value& value)
{
    if (const int* intValue = std::get_if<int>(&value)) {
        return *intValue;
    }
    if (const double* doubleValue = std::get_if<double>(&value)) {
        return static_cast<int>(*doubleValue);
    }
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return *boolValue ? 1 : 0;
    }
    return 0;
}

static std::string AsString(const Value& value)
{
    if (const std::string* stringValue = std::get_if<std::string>(&value)) {
        return *stringValue;
    }
    return ValueToString(value);
}

static void ExecEventBegin(ExecContext& context)
{
    context.RunExecOutput(0);
}

// Inputs: [exec, Condition]; outputs: [True, False].
static void ExecBranch(ExecContext& context)
{
    context.RunExecOutput(AsBool(context.GetInput(1)) ? 0 : 1);
}

// Outputs: [Then 0, Then 1].
static void ExecSequence(ExecContext& context)
{
    context.RunExecOutput(0);
    context.RunExecOutput(1);
}

// Inputs: [exec, In String]; outputs: [exec].
static void ExecPrintString(ExecContext& context)
{
    context.Log(AsString(context.GetInput(1)));
    context.RunExecOutput(0);
}

// Inputs: [A, B]; outputs: [Result].
static void ExecAdd(ExecContext& context)
{
    context.SetOutput(0, Value(AsInt(context.GetInput(0)) + AsInt(context.GetInput(1))));
}

// Inputs: [A, B]; outputs: [Result]. Equality comparison.
static void ExecCompare(ExecContext& context)
{
    context.SetOutput(0, Value(AsInt(context.GetInput(0)) == AsInt(context.GetInput(1))));
}

// Inputs: [exec, First Index, Last Index];
// outputs: [Loop Body, Index, Completed]. Inclusive range like UE.
static void ExecForLoop(ExecContext& context)
{
    const int firstIndex = AsInt(context.GetInput(1));
    const int lastIndex = AsInt(context.GetInput(2));
    for (int i = firstIndex; i <= lastIndex; ++i) {
        context.SetOutput(1, Value(i));
        context.RunExecOutput(0);
    }
    context.RunExecOutput(2);
}

// Formats all non-exec inputs into the first string output. The first
// property is an optional format template: "{n}" placeholders are
// replaced by input n's value; an empty template joins the values with
// ", ".
static void ExecMakeString(ExecContext& context)
{
    const Node& node = context.GetNode();

    std::vector<std::string> inputTexts;
    for (int i = 0; i < static_cast<int>(node.inputs.size()); ++i) {
        if (node.inputs[static_cast<std::size_t>(i)].type != PinType::Exec) {
            inputTexts.push_back(AsString(context.GetInput(i)));
        }
    }

    std::string result = AsString(context.GetProperty(0));
    if (result.empty()) {
        for (const std::string& text : inputTexts) {
            if (!result.empty()) {
                result += ", ";
            }
            result += text;
        }
    } else {
        for (int i = 0; i < static_cast<int>(inputTexts.size()); ++i) {
            const std::string placeholder = "{" + std::to_string(i) + "}";
            std::size_t position = 0;
            while ((position = result.find(placeholder, position)) != std::string::npos) {
                result.replace(position, placeholder.size(),
                               inputTexts[static_cast<std::size_t>(i)]);
                position += inputTexts[static_cast<std::size_t>(i)].size();
            }
        }
    }

    for (int outputIndex = 0; outputIndex < static_cast<int>(node.outputs.size());
         ++outputIndex) {
        if (node.outputs[static_cast<std::size_t>(outputIndex)].type == PinType::String) {
            context.SetOutput(outputIndex, Value(result));
            break;
        }
    }
}

struct ExecFnEntry
{
    const char* name;
    NodeExecFn fn;
};

static const ExecFnEntry EXEC_FN_TABLE[] = {
    {"Event Begin", ExecEventBegin},
    {"Branch", ExecBranch},
    {"Sequence", ExecSequence},
    {"Print String", ExecPrintString},
    {"Add", ExecAdd},
    {"Compare", ExecCompare},
    {"For Loop", ExecForLoop},
    {"MakeString", ExecMakeString},
};

NodeExecFn FindNodeExecFn(const char* name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (const ExecFnEntry& entry : EXEC_FN_TABLE) {
        if (std::strcmp(entry.name, name) == 0) {
            return entry.fn;
        }
    }
    return nullptr;
}
