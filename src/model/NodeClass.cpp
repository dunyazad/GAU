#include "NodeClass.h"

#include <cstring>
#include <utility>

NodeClass::NodeClass(std::string name, NodeCategory category, std::vector<PinDef> pins)
    : name(std::move(name))
    , category(category)
    , pins(std::move(pins))
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
        MutableDynamicStorage().push_back(std::move(nodeClass));
    }
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
