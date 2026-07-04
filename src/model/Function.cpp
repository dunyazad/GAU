// FunctionRegistry: owns function definitions by stable address.

#include "Function.h"

#include <utility>

namespace gau {

FunctionDef* FunctionRegistry::Create(const TypeRegistry& types, std::string name)
{
    if (Find(name) != nullptr) {
        return nullptr;
    }
    auto def = std::make_unique<FunctionDef>(types);
    def->name = std::move(name);
    FunctionDef* ptr = def.get();
    defs.push_back(std::move(def));
    return ptr;
}

const FunctionDef* FunctionRegistry::Find(const std::string& name) const
{
    for (const auto& def : defs) {
        if (def->name == name) {
            return def.get();
        }
    }
    return nullptr;
}

FunctionDef* FunctionRegistry::Find(const std::string& name)
{
    for (auto& def : defs) {
        if (def->name == name) {
            return def.get();
        }
    }
    return nullptr;
}

} // namespace gau
