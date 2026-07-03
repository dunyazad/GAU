#include "NodeClass.h"

#include <cstring>
#include <utility>

NodeClass::NodeClass(std::string name, std::string category, std::vector<PinDef> pins,
                     std::vector<PropertyDef> properties, std::string execFnName)
    : name(std::move(name))
    , category(std::move(category))
    , pins(std::move(pins))
    , properties(std::move(properties))
    , execFnName(std::move(execFnName))
{
    MutableRegistry().push_back(this);
}

const std::vector<const NodeClass*>& NodeClass::GetRegistry()
{
    return MutableRegistry();
}

const NodeClass* NodeClass::FindByName(const char* name)
{
    if (name == nullptr) {
        return nullptr;
    }
    for (const NodeClass* nodeClass : MutableRegistry()) {
        if (std::strcmp(nodeClass->name.c_str(), name) == 0) {
            return nodeClass;
        }
    }
    return nullptr;
}

void NodeClass::AdoptDynamic(std::unique_ptr<NodeClass> nodeClass)
{
    if (nodeClass != nullptr) {
        nodeClass->dynamic = true;
        MutableDynamicStorage().push_back(std::move(nodeClass));
    }
}

bool NodeClass::UpdateDynamic(const NodeClass* target, std::string newName,
                              std::string newCategory, std::vector<PinDef> newPins,
                              std::vector<PropertyDef> newProperties,
                              std::string newExecFnName)
{
    for (std::unique_ptr<NodeClass>& stored : MutableDynamicStorage()) {
        if (stored.get() == target) {
            stored->name = std::move(newName);
            stored->category = std::move(newCategory);
            stored->pins = std::move(newPins);
            stored->properties = std::move(newProperties);
            stored->execFnName = std::move(newExecFnName);
            return true;
        }
    }
    return false;
}

std::vector<const NodeClass*>& NodeClass::MutableRegistry()
{
    // Construct-on-first-use: safe against static initialization order
    // because self-registering NodeClass constructors call this first.
    static std::vector<const NodeClass*> registry;
    return registry;
}

std::vector<std::unique_ptr<NodeClass>>& NodeClass::MutableDynamicStorage()
{
    static std::vector<std::unique_ptr<NodeClass>> storage;
    return storage;
}
