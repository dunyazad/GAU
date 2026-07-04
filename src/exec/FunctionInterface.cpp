// Function interface editing: append a parameter and reconcile instances.

#include "FunctionInterface.h"

#include "FunctionNodes.h"

#include <string>

namespace gau {

void AddFunctionParam(FunctionDef& def, bool isOutput, const std::string& name, TypeId type,
                      NodeClassRegistry& classes, BuiltinRegistry& builtins, TypeRegistry& types,
                      Project& project)
{
    if (isOutput) {
        def.outputs.push_back(FunctionParam{name, type});
    } else {
        def.inputs.push_back(FunctionParam{name, type});
    }
    // Regenerate the class defs/behaviors so new instances and the runtime see
    // the added parameter.
    RegisterFunctionNodes(classes, builtins, types, def);

    const std::string entryName = def.name + " In";
    const std::string returnName = def.name + " Out";
    const std::string callName = def.name;

    // Append the matching pin to existing instances. Appending keeps existing
    // pin ids (and their links) intact; the new pin lands last, matching the
    // regenerated class layout.
    const auto applyTo = [&](Graph& graph) {
        for (const Node& n : graph.Nodes()) {
            if (n.className == callName) {
                graph.AppendPin(n.id, isOutput ? PinDirection::Output : PinDirection::Input, type,
                                name);
            } else if (!isOutput && n.className == entryName) {
                graph.AppendPin(n.id, PinDirection::Output, type, name);
            } else if (isOutput && n.className == returnName) {
                graph.AppendPin(n.id, PinDirection::Input, type, name);
            }
        }
    };

    applyTo(*project.graph);
    for (const auto& fp : project.functions.All()) {
        if (fp->body) {
            applyTo(*fp->body);
        }
    }
}

} // namespace gau
