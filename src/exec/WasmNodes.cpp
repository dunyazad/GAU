// Wasm-bound node class construction (execFn "wasm:<name>").

#include "WasmNodes.h"

#include <utility>

namespace gau {

NodeClass MakeWasmNodeClass(std::string name, std::string category, std::vector<PinDef> pins)
{
    NodeClass c;
    c.execFn = "wasm:" + name;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    return c;
}

void RegisterWasmNodeClass(NodeClassRegistry& classes, std::string name, std::string category,
                           std::vector<PinDef> pins)
{
    classes.Register(MakeWasmNodeClass(std::move(name), std::move(category), std::move(pins)));
}

void RegisterWasmNodeClass(NodeClassRegistry& classes, std::string name, std::string category,
                           std::vector<PinDef> pins, std::string execFn)
{
    NodeClass c;
    c.name = std::move(name);
    c.category = std::move(category);
    c.pins = std::move(pins);
    c.execFn = std::move(execFn);
    classes.Register(std::move(c));
}

} // namespace gau
