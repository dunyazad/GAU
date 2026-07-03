#pragma once

#include "GraphTypes.h"

#include <memory>
#include <string>
#include <vector>

struct PinDef
{
    PinDirection direction = PinDirection::Input;
    PinType type = PinType::Exec;
    std::string name;
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
    NodeClass(std::string name, NodeCategory category, std::vector<PinDef> pins);

    NodeClass(const NodeClass&) = delete;
    NodeClass& operator=(const NodeClass&) = delete;

    const char* GetName() const { return name.c_str(); }
    NodeCategory GetCategory() const { return category; }
    const std::vector<PinDef>& GetPins() const { return pins; }

    // All registered node classes, in registration (definition) order.
    static const std::vector<const NodeClass*>& GetRegistry();

    // Case-sensitive lookup by class name. Returns nullptr if unknown.
    static const NodeClass* FindByName(const char* name);

    // Takes ownership of a dynamically created class (e.g. loaded from
    // JSON). The instance already registered itself in its constructor;
    // callers must check FindByName for duplicates BEFORE constructing.
    static void AdoptDynamic(std::unique_ptr<NodeClass> nodeClass);

private:
    static std::vector<const NodeClass*>& MutableRegistry();
    static std::vector<std::unique_ptr<NodeClass>>& MutableDynamicStorage();

    std::string name;
    NodeCategory category;
    std::vector<PinDef> pins;
};
