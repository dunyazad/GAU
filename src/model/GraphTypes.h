#pragma once

#include <cctype>
#include <cstdint>
#include <string>

// Pure data types shared across the model layer.
// This layer must stay free of SDL/rendering includes.

using NodeId = std::uint32_t;
using PinId = std::uint32_t;
using LinkId = std::uint32_t;

constexpr std::uint32_t INVALID_ID = 0;

enum class PinType
{
    Exec,
    Bool,
    Int,
    Float,
    String,
    Object,
};

enum class PinDirection
{
    Input,
    Output,
};

enum class NodeCategory
{
    Event,
    Function,
    FlowControl,
    Pure,
};

constexpr int PIN_TYPE_COUNT = 6;

constexpr PinType ALL_PIN_TYPES[PIN_TYPE_COUNT] = {
    PinType::Exec,
    PinType::Bool,
    PinType::Int,
    PinType::Float,
    PinType::String,
    PinType::Object,
};

// Serialized (JSON schema) names: lowercase.
inline const char* PinTypeToString(PinType type)
{
    switch (type) {
    case PinType::Exec:
        return "exec";
    case PinType::Bool:
        return "bool";
    case PinType::Int:
        return "int";
    case PinType::Float:
        return "float";
    case PinType::String:
        return "string";
    case PinType::Object:
        return "object";
    }
    return "";
}

inline const char* PinDirectionToString(PinDirection direction)
{
    return direction == PinDirection::Input ? "in" : "out";
}

constexpr int NODE_CATEGORY_COUNT = 4;

constexpr NodeCategory ALL_NODE_CATEGORIES[NODE_CATEGORY_COUNT] = {
    NodeCategory::Event,
    NodeCategory::Function,
    NodeCategory::FlowControl,
    NodeCategory::Pure,
};

inline int NodeCategoryIndex(NodeCategory category)
{
    switch (category) {
    case NodeCategory::Event:
        return 0;
    case NodeCategory::Function:
        return 1;
    case NodeCategory::FlowControl:
        return 2;
    case NodeCategory::Pure:
        return 3;
    }
    return 0;
}

inline const char* NodeCategoryToString(NodeCategory category)
{
    switch (category) {
    case NodeCategory::Event:
        return "Event";
    case NodeCategory::Function:
        return "Function";
    case NodeCategory::FlowControl:
        return "FlowControl";
    case NodeCategory::Pure:
        return "Pure";
    }
    return "";
}

// Case-insensitive (ASCII) name lookup. Returns false for unknown names.
inline bool NodeCategoryFromString(const std::string& text, NodeCategory& outCategory)
{
    std::string lower = text;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lower == "event") {
        outCategory = NodeCategory::Event;
    } else if (lower == "function") {
        outCategory = NodeCategory::Function;
    } else if (lower == "flowcontrol") {
        outCategory = NodeCategory::FlowControl;
    } else if (lower == "pure") {
        outCategory = NodeCategory::Pure;
    } else {
        return false;
    }
    return true;
}
