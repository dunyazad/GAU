#pragma once

// Interns types and holds user type definitions. Resolves TypeIds, builds
// container types, and produces default values. Pure data.

#include "Type.h"
#include "Value.h"

#include <string>
#include <vector>

namespace gau {

class TypeRegistry
{
public:
    TypeRegistry();

    // Builtin scalar type id for Exec/Bool/Int/Float/String/Object.
    TypeId Builtin(TypeTag tag) const;

    // Interns a type description, returning an existing id when identical.
    TypeId Intern(const TypeDesc& desc);
    TypeId ArrayOf(TypeId element);
    TypeId SetOf(TypeId element);
    TypeId MapOf(TypeId key, TypeId value);
    // Enum/struct by name (defined via DefineEnum/DefineStruct), else an
    // object alias of that name.
    TypeId UserType(const std::string& name);

    const TypeDesc* Resolve(TypeId id) const;

    void DefineEnum(EnumDef def);
    void DefineStruct(StructDef def);
    const EnumDef* FindEnum(const std::string& name) const;
    const StructDef* FindStruct(const std::string& name) const;
    const std::vector<EnumDef>& Enums() const { return enums; }
    const std::vector<StructDef>& Structs() const { return structs; }

    // Zero value for a type: scalars/enum, empty containers, or a struct
    // with each field defaulted (recursive).
    Value MakeDefault(TypeId id) const;

    // Display name: builtin keyword, user type name, or a composed
    // container name like "Array<Vector3f>".
    std::string TypeName(TypeId id) const;

private:
    std::vector<TypeDesc> descs; // index 0 is the invalid sentinel
    std::vector<EnumDef> enums;
    std::vector<StructDef> structs;
    TypeId builtinIds[6] = {};
};

} // namespace gau
