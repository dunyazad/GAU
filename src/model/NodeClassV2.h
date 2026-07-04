#pragma once

// v2 node class: a node type definition. Pins and properties reference
// core TypeIds; property defaults are core Values. Classes are built
// against a TypeRegistry (owned by the Project) and stored by name in a
// NodeClassRegistry. (File is NodeClassV2 to coexist with the v1
// NodeClass.h during migration; type lives in namespace gau.)

#include "Ids.h"

#include "core/Type.h"
#include "core/Value.h"

#include <string>
#include <vector>

namespace gau {

struct PinDef
{
    PinDirection direction = PinDirection::Input;
    TypeId type = INVALID_TYPE;
    std::string name;
};

struct PropertyDef
{
    std::string name;
    TypeId type = INVALID_TYPE;
    Value defaultValue;
};

struct NodeClass
{
    std::string name;
    std::string category;
    std::vector<PinDef> pins;
    std::vector<PropertyDef> properties;
    // Native/Wasm exec binding; empty means default passthrough.
    std::string execFn;
};

// Registry of node classes, owned per project. Case-sensitive lookup by
// name; registering an existing name replaces it.
class NodeClassRegistry
{
public:
    void Register(NodeClass nodeClass);
    const NodeClass* Find(const std::string& name) const;
    const std::vector<NodeClass>& All() const { return classes; }
    void Clear() { classes.clear(); }

private:
    std::vector<NodeClass> classes;
};

} // namespace gau
