#pragma once

// v2 node and pin. Pins carry a core TypeId; property values are core
// Values, so structs and containers need no special-case storage.

#include "Ids.h"

#include "core/Type.h"
#include "core/Value.h"

#include <string>
#include <vector>

namespace gau {

struct Pin
{
    PinId id = INVALID_ID;
    NodeId node = INVALID_ID;
    PinDirection direction = PinDirection::Input;
    TypeId type = INVALID_TYPE;
    std::string name;
};

struct Node
{
    NodeId id = INVALID_ID;
    // Stable identity across save/load and undo.
    std::string guid;
    // Name of the NodeClass this instance was created from.
    std::string className;
    float x = 0.0f;
    float y = 0.0f;
    std::vector<Pin> inputs;
    std::vector<Pin> outputs;
    // Parallel to the class property definitions.
    std::vector<Value> properties;
};

} // namespace gau
