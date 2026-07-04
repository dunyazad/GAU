// Get/Set node generation for local variables.

#include "VariableNodes.h"

#include "core/Value.h"

#include <string>
#include <variant>

namespace gau {

void RegisterVariableNodes(NodeClassRegistry& classes, BuiltinRegistry& builtins,
                           TypeRegistry& types, const VariableDef& def)
{
    const TypeId execId = types.Builtin(TypeTag::Exec);

    // Get <var>: pure read.
    NodeClass get;
    get.name = "Get " + def.name;
    get.category = "Pure";
    get.pins.push_back(PinDef{PinDirection::Output, def.type, def.name});
    classes.Register(get);

    // Set <var>: exec write.
    NodeClass set;
    set.name = "Set " + def.name;
    set.category = "FlowControl";
    set.pins.push_back(PinDef{PinDirection::Input, execId, "Exec"});
    set.pins.push_back(PinDef{PinDirection::Input, def.type, def.name});
    set.pins.push_back(PinDef{PinDirection::Output, execId, "Then"});
    classes.Register(set);

    const std::string name = def.name;
    const TypeId varType = def.type;
    TypeRegistry* typesPtr = &types;
    builtins.Register(get.name, [name, varType, typesPtr](NodeEval& e) {
        Value v = e.rt->GetVariable(name);
        if (std::holds_alternative<std::monostate>(v.data)) {
            v = typesPtr->MakeDefault(varType);
        }
        e.Out(0, std::move(v));
    });
    builtins.Register(set.name, [name](NodeEval& e) { e.rt->SetVariable(name, e.In(1)); });
}

} // namespace gau
