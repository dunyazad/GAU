#pragma once

#include "GraphTypes.h"

#include <string>
#include <vector>

// User-defined data types, selectable anywhere a PinType is chosen. The
// registry is global (types are shared across graphs, like NodeClass) and
// pure data: no rendering/SDL includes.

enum class UserTypeKind
{
    Enum,
    Struct,
    ObjectAlias,
};

// One field of a struct user type. type/typeName follow the same rule as
// pins: typeName is meaningful only when type == PinType::UserType.
struct StructField
{
    std::string name;
    PinType type = PinType::Float;
    std::string typeName;
};

struct UserType
{
    std::string name;
    UserTypeKind kind = UserTypeKind::Enum;
    // Enumerator names (kind == Enum).
    std::vector<std::string> enumerators;
    // Fields (kind == Struct).
    std::vector<StructField> fields;
};

// Serialized (JSON) names: lowercase.
inline const char* UserTypeKindToString(UserTypeKind kind)
{
    switch (kind) {
    case UserTypeKind::Enum:
        return "enum";
    case UserTypeKind::Struct:
        return "struct";
    case UserTypeKind::ObjectAlias:
        return "object";
    }
    return "";
}

inline bool UserTypeKindFromString(const std::string& text, UserTypeKind& outKind)
{
    if (text == "enum") {
        outKind = UserTypeKind::Enum;
    } else if (text == "struct") {
        outKind = UserTypeKind::Struct;
    } else if (text == "object" || text == "objectalias") {
        outKind = UserTypeKind::ObjectAlias;
    } else {
        return false;
    }
    return true;
}

// Global registry of user-defined types, in insertion order.
class UserTypeRegistry
{
public:
    static const std::vector<UserType>& GetAll();
    // Case-sensitive lookup by type name; nullptr if unknown.
    static const UserType* Find(const std::string& name);
    // Adds a new type, or replaces the existing one with the same name.
    static void Register(UserType type);
    // Removes a type by name; returns false if not present.
    static bool Remove(const std::string& name);
    static void Clear();

private:
    static std::vector<UserType>& Mutable();
};
