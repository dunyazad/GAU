// Demo node behaviors for the v2 runtime.

#include "Builtins.h"

#include "core/Value.h"

#include <cstdint>
#include <variant>

namespace gau {

static std::int64_t AsInt(const Value& value)
{
    if (const std::int64_t* v = std::get_if<std::int64_t>(&value.data)) {
        return *v;
    }
    if (const double* v = std::get_if<double>(&value.data)) {
        return static_cast<std::int64_t>(*v);
    }
    return 0;
}

static bool AsBool(const Value& value)
{
    if (const bool* v = std::get_if<bool>(&value.data)) {
        return *v;
    }
    return false;
}

void RegisterDemoBuiltins(BuiltinRegistry& registry)
{
    // Entry: exec-only. Default next follows the first exec output.
    registry.Register("EventBegin", [](NodeEval&) {});

    // Literals: property -> output.
    registry.Register("MakeInt", [](NodeEval& e) { e.Out(0, e.Prop(0)); });
    registry.Register("MakeBool", [](NodeEval& e) { e.Out(0, e.Prop(0)); });

    // Pure arithmetic: inputs A(0), B(1) -> Result(0).
    registry.Register("Add", [](NodeEval& e) {
        e.Out(0, Value::Int(AsInt(e.In(0)) + AsInt(e.In(1))));
    });

    // Print: exec(0), Value(1). Logs the int, then continues.
    registry.Register("PrintInt", [](NodeEval& e) {
        e.rt->Log(ValueToString(e.In(1)));
    });

    // Branch: exec(0), Cond(1) -> True(0) / False(1) exec outputs.
    registry.Register("Branch", [](NodeEval& e) {
        if (AsBool(e.In(1))) {
            e.Then(0);
        } else {
            e.Then(1);
        }
    });
}

} // namespace gau
