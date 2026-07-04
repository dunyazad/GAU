// Struct field editing: add/remove a field and reconcile Make/Break instances.

#include "StructEdit.h"

#include "StructNodes.h"

#include <cstddef>
#include <string>

namespace gau {

void AddStructField(TypeRegistry& types, const std::string& structName,
                    const std::string& fieldName, TypeId fieldType, NodeClassRegistry& classes,
                    BuiltinRegistry& builtins, Project& project)
{
    const StructDef* current = types.FindStruct(structName);
    if (current == nullptr) {
        return;
    }
    // Copy before redefining (DefineStruct replaces the entry in place).
    StructDef def = *current;
    def.fields.push_back(StructField{fieldName, fieldType});
    types.DefineStruct(def);
    // Regenerate Make/Break classes/behaviors so new instances and the runtime
    // see the added field.
    RegisterStructNodes(classes, builtins, types, def);

    const std::string makeName = "Make " + structName;
    const std::string breakName = "Break " + structName;

    // Append the matching pin to existing instances. Appending keeps existing
    // pin ids (and their links) intact; the new pin lands last, matching the
    // regenerated class layout (Make field inputs, Break field outputs).
    const auto applyTo = [&](Graph& graph) {
        for (const Node& n : graph.Nodes()) {
            if (n.className == makeName) {
                graph.AppendPin(n.id, PinDirection::Input, fieldType, fieldName);
            } else if (n.className == breakName) {
                graph.AppendPin(n.id, PinDirection::Output, fieldType, fieldName);
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

void RemoveStructField(TypeRegistry& types, const std::string& structName, int index,
                       NodeClassRegistry& classes, BuiltinRegistry& builtins, Project& project)
{
    const StructDef* current = types.FindStruct(structName);
    if (current == nullptr || index < 0 || index >= static_cast<int>(current->fields.size())) {
        return;
    }
    const std::size_t pos = static_cast<std::size_t>(index);
    const std::string makeName = "Make " + structName;
    const std::string breakName = "Break " + structName;

    // Drop the positional pin from each instance before erasing the field:
    // Make field i is inputs[i]; Break field i is outputs[i].
    const auto applyTo = [&](Graph& graph) {
        for (const Node& n : graph.Nodes()) {
            if (n.className == makeName) {
                if (pos < n.inputs.size()) {
                    graph.RemovePin(n.id, n.inputs[pos].id);
                }
            } else if (n.className == breakName) {
                if (pos < n.outputs.size()) {
                    graph.RemovePin(n.id, n.outputs[pos].id);
                }
            }
        }
    };
    applyTo(*project.graph);
    for (const auto& fp : project.functions.All()) {
        if (fp->body) {
            applyTo(*fp->body);
        }
    }

    StructDef def = *current;
    def.fields.erase(def.fields.begin() + index);
    types.DefineStruct(def);
    RegisterStructNodes(classes, builtins, types, def);
}

} // namespace gau
