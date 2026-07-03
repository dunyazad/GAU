#pragma once

#include "GraphTypes.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct PinDef
{
    PinDirection direction = PinDirection::Input;
    PinType type = PinType::Exec;
    std::string name;
};

// Class-level property definition (analogue of a UPROPERTY): a typed
// field with a default value, optionally shaped as an STL container
// (Array = std::vector, Set = unique elements, Map = key-value pairs).
// Node instances copy the defaults into their own value storage at
// spawn time. Element/key/value types are restricted to value types
// (Bool/Int/Float/String); Exec/Object are not allowed.
struct PropertyDef
{
    std::string name;
    PropertyContainer container = PropertyContainer::None;
    // Element type (Array/Set), value type (Map), or the scalar type.
    PinType type = PinType::Float;
    // Key type; meaningful only when container == Map.
    PinType keyType = PinType::String;
    // Default for container == None.
    Value defaultValue;
    // Defaults for Array/Set.
    std::vector<Value> defaultElements;
    // Defaults for Map.
    std::vector<std::pair<Value, Value>> defaultEntries;
};

// Per-node-instance storage for one property, mirroring PropertyDef's
// container shape.
struct PropertyValue
{
    Value scalar;
    std::vector<Value> elements;
    std::vector<std::pair<Value, Value>> entries;
};

// Runtime meta-object describing a node type (analogue of UE's UClass).
// Static NodeClass instances self-register into a global registry during
// static initialization; JSON-loaded classes are heap instances whose
// ownership is adopted via AdoptDynamic. The registry drives the
// node-creation menu and name-based lookup for serialization (M5).
// Execution behavior (M5) will be added here as a function slot.
class NodeClass
{
public:
    NodeClass(std::string name, std::string category, std::vector<PinDef> pins,
              std::vector<PropertyDef> properties = {}, std::string execFnName = {});

    NodeClass(const NodeClass&) = delete;
    NodeClass& operator=(const NodeClass&) = delete;

    const char* GetName() const { return name.c_str(); }
    const std::string& GetCategory() const { return category; }
    const std::vector<PinDef>& GetPins() const { return pins; }
    const std::vector<PropertyDef>& GetProperties() const { return properties; }
    // Name of the native exec function bound to this class (JSON
    // "execFn"). Empty means: look up by class name, else the default
    // passthrough behavior.
    const std::string& GetExecFnName() const { return execFnName; }

    // True for classes registered at runtime (JSON file or the class
    // editor dialog); builtin static classes return false and cannot
    // be edited.
    bool IsDynamic() const { return dynamic; }

    // All registered node classes, in registration (definition) order.
    static const std::vector<const NodeClass*>& GetRegistry();

    // Case-sensitive lookup by class name. Returns nullptr if unknown.
    static const NodeClass* FindByName(const char* name);

    // Takes ownership of a dynamically created class (e.g. loaded from
    // JSON). The instance already registered itself in its constructor;
    // callers must check FindByName for duplicates BEFORE constructing.
    static void AdoptDynamic(std::unique_ptr<NodeClass> nodeClass);

    // Replaces the definition of a dynamic class in place; existing
    // Node instances keep a valid pointer. Returns false for builtin
    // (non-dynamic) classes. Callers must re-sync spawned nodes via
    // NodeGraph::RebuildNodesOfClass afterwards.
    static bool UpdateDynamic(const NodeClass* target, std::string newName,
                              std::string newCategory, std::vector<PinDef> newPins,
                              std::vector<PropertyDef> newProperties,
                              std::string newExecFnName);

private:
    static std::vector<const NodeClass*>& MutableRegistry();
    static std::vector<std::unique_ptr<NodeClass>>& MutableDynamicStorage();

    std::string name;
    std::string category;
    std::vector<PinDef> pins;
    std::vector<PropertyDef> properties;
    std::string execFnName;
    bool dynamic = false;
};
