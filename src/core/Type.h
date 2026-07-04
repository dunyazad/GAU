#pragma once

// Core type system for v2: interned, composable types shared by model,
// exec and render. Pure data -- no SDL/render/model dependencies.

#include <cstdint>
#include <string>
#include <vector>

namespace gau {

using TypeId = std::uint32_t;
constexpr TypeId INVALID_TYPE = 0;

enum class TypeTag
{
    Exec,
    Bool,
    Int,
    Float,
    String,
    Object,
    Enum,
    Struct,
    Array,
    Set,
    Map,
};

// Interned type description. Builtins use tag only; user types carry a
// name; containers reference element/key TypeIds so nested types like
// Array<Vector3f> or Map<String, Vector3f> compose freely.
struct TypeDesc
{
    TypeTag tag = TypeTag::Exec;
    std::string name;                 // Enum/Struct/Object user type name
    TypeId element = INVALID_TYPE;    // Array/Set element, or Map value
    TypeId key = INVALID_TYPE;        // Map key
};

struct StructField
{
    std::string name;
    TypeId type = INVALID_TYPE;
};

struct EnumDef
{
    std::string name;
    std::vector<std::string> values;
};

struct StructDef
{
    std::string name;
    std::vector<StructField> fields;
};

const char* TypeTagToString(TypeTag tag);

} // namespace gau
