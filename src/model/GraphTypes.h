#pragma once

#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Pure data types shared across the model layer.
// This layer must stay free of SDL/rendering includes.

using NodeId = std::uint32_t;
using PinId = std::uint32_t;
using LinkId = std::uint32_t;
using CommentId = std::uint32_t;

constexpr std::uint32_t INVALID_ID = 0;

// Link bezier tangent from the design spec; shared by rendering and
// link hit testing so both trace the same curve.
inline float LinkTangent(float dx)
{
    const float absDx = std::fabs(dx);
    return (absDx < 200.0f) ? absDx * 0.5f + 50.0f : absDx * 0.5f;
}

// Comment (group) box geometry, canvas units at zoom 1.
constexpr float COMMENT_TITLE_HEIGHT = 30.0f;
constexpr float COMMENT_RESIZE_HANDLE = 16.0f;
constexpr float COMMENT_MIN_WIDTH = 120.0f;
constexpr float COMMENT_MIN_HEIGHT = 80.0f;
constexpr float COMMENT_DEFAULT_WIDTH = 320.0f;
constexpr float COMMENT_DEFAULT_HEIGHT = 200.0f;
constexpr float COMMENT_WRAP_PADDING = 20.0f;

// Runtime value for data pins and node properties (exec-engine spec).
// Alternatives map to PinType: Bool->bool, Int->int, Float->double,
// String->std::string.
using Value = std::variant<bool, int, double, std::string>;

// Evaluated output-pin values keyed by PinId. Produced by the exec
// engine's display preview and read by the renderer to show live values
// on linked node property rows.
using PinValueCache = std::vector<std::pair<PinId, Value>>;

enum class PinType
{
    Exec,
    Bool,
    Int,
    Float,
    String,
    Object,
    // References a user-defined type (enum/struct/object alias). The
    // concrete type is named by a companion typeName string on the pin,
    // pin def or property; look it up in UserTypeRegistry.
    UserType,
};

enum class PinDirection
{
    Input,
    Output,
};

// Node categories are free-form strings (users can define new ones).
// The builtin names below get fixed palette colors and come first in
// menu ordering; unknown categories get a hash-derived color.
constexpr int BUILTIN_CATEGORY_COUNT = 6;

constexpr const char* BUILTIN_CATEGORY_NAMES[BUILTIN_CATEGORY_COUNT] = {
    "Event",
    "Function",
    "FlowControl",
    "Pure",
    "Object",
    "CustomObject",
};

// Container shape of a node property (STL-backed): scalar, std::vector,
// set (unique elements) or key-value map.
enum class PropertyContainer
{
    None,
    Array,
    Set,
    Map,
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

// Types a property (element/key/value) may have: value types only.
constexpr int VALUE_PIN_TYPE_COUNT = 4;

constexpr PinType VALUE_PIN_TYPES[VALUE_PIN_TYPE_COUNT] = {
    PinType::Bool,
    PinType::Int,
    PinType::Float,
    PinType::String,
};

inline int ValuePinTypeIndex(PinType type)
{
    for (int i = 0; i < VALUE_PIN_TYPE_COUNT; ++i) {
        if (VALUE_PIN_TYPES[i] == type) {
            return i;
        }
    }
    return 0;
}

constexpr int PROPERTY_CONTAINER_COUNT = 4;

constexpr PropertyContainer ALL_PROPERTY_CONTAINERS[PROPERTY_CONTAINER_COUNT] = {
    PropertyContainer::None,
    PropertyContainer::Array,
    PropertyContainer::Set,
    PropertyContainer::Map,
};

inline int PropertyContainerIndex(PropertyContainer container)
{
    switch (container) {
    case PropertyContainer::None:
        return 0;
    case PropertyContainer::Array:
        return 1;
    case PropertyContainer::Set:
        return 2;
    case PropertyContainer::Map:
        return 3;
    }
    return 0;
}

// Serialized (JSON schema) names: lowercase.
inline const char* PropertyContainerToString(PropertyContainer container)
{
    switch (container) {
    case PropertyContainer::None:
        return "none";
    case PropertyContainer::Array:
        return "array";
    case PropertyContainer::Set:
        return "set";
    case PropertyContainer::Map:
        return "map";
    }
    return "";
}

inline bool PropertyContainerFromString(const std::string& text, PropertyContainer& outContainer)
{
    std::string lower = text;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lower.empty() || lower == "none" || lower == "scalar") {
        outContainer = PropertyContainer::None;
    } else if (lower == "array" || lower == "vector") {
        outContainer = PropertyContainer::Array;
    } else if (lower == "set") {
        outContainer = PropertyContainer::Set;
    } else if (lower == "map") {
        outContainer = PropertyContainer::Map;
    } else {
        return false;
    }
    return true;
}

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
    case PinType::UserType:
        return "usertype";
    }
    return "";
}

inline const char* PinDirectionToString(PinDirection direction)
{
    return direction == PinDirection::Input ? "in" : "out";
}

// Case-insensitive (ASCII) pin type lookup ("exec"/"bool"/"int"/
// "float"/"string"/"object"). Returns false for unknown names.
inline bool PinTypeFromString(const std::string& text, PinType& outType)
{
    std::string lower = text;
    for (char& c : lower) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (lower == "exec") {
        outType = PinType::Exec;
    } else if (lower == "bool") {
        outType = PinType::Bool;
    } else if (lower == "int") {
        outType = PinType::Int;
    } else if (lower == "float") {
        outType = PinType::Float;
    } else if (lower == "string") {
        outType = PinType::String;
    } else if (lower == "object") {
        outType = PinType::Object;
    } else if (lower == "usertype") {
        outType = PinType::UserType;
    } else {
        return false;
    }
    return true;
}

// Zero value of the Value alternative matching a pin/property type.
// Exec/Object have no value representation and map to false/0-like bool.
inline Value MakeDefaultValue(PinType type)
{
    switch (type) {
    case PinType::Bool:
        return Value(false);
    case PinType::Int:
        return Value(0);
    case PinType::Float:
        return Value(0.0);
    case PinType::String:
        return Value(std::string());
    case PinType::UserType:
        // Enum values map to their enumerator index; struct/object alias
        // have no runtime value yet and fall back to 0.
        return Value(0);
    case PinType::Exec:
    case PinType::Object:
        break;
    }
    return Value(false);
}

// Parses user-entered text into a Value of the given type. Empty text
// yields the type's default. Returns false on malformed input.
inline bool ParseValueString(const std::string& text, PinType type, Value& outValue)
{
    if (text.empty()) {
        outValue = MakeDefaultValue(type);
        return true;
    }
    switch (type) {
    case PinType::Bool:
        if (text == "true" || text == "1") {
            outValue = Value(true);
            return true;
        }
        if (text == "false" || text == "0") {
            outValue = Value(false);
            return true;
        }
        return false;
    case PinType::UserType:
    case PinType::Int: {
        char* end = nullptr;
        const long parsed = std::strtol(text.c_str(), &end, 10);
        if (end == nullptr || *end != '\0') {
            return false;
        }
        outValue = Value(static_cast<int>(parsed));
        return true;
    }
    case PinType::Float: {
        char* end = nullptr;
        const double parsed = std::strtod(text.c_str(), &end);
        if (end == nullptr || *end != '\0') {
            return false;
        }
        outValue = Value(parsed);
        return true;
    }
    case PinType::String:
        outValue = Value(text);
        return true;
    case PinType::Exec:
    case PinType::Object:
        break;
    }
    return false;
}

inline std::string ValueToString(const Value& value)
{
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return *boolValue ? "true" : "false";
    }
    if (const int* intValue = std::get_if<int>(&value)) {
        return std::to_string(*intValue);
    }
    if (const double* doubleValue = std::get_if<double>(&value)) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%g", *doubleValue);
        return std::string(buffer);
    }
    if (const std::string* stringValue = std::get_if<std::string>(&value)) {
        return *stringValue;
    }
    return "";
}
