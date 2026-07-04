// v2 node class registry.

#include "NodeClassV2.h"

#include <utility>

namespace gau {

void NodeClassRegistry::Register(NodeClass nodeClass)
{
    for (NodeClass& existing : classes) {
        if (existing.name == nodeClass.name) {
            existing = std::move(nodeClass);
            return;
        }
    }
    classes.push_back(std::move(nodeClass));
}

const NodeClass* NodeClassRegistry::Find(const std::string& name) const
{
    for (const NodeClass& nodeClass : classes) {
        if (nodeClass.name == name) {
            return &nodeClass;
        }
    }
    return nullptr;
}

} // namespace gau
