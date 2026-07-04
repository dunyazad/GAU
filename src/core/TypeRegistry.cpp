// Type interning, user type storage, defaults and display names.

#include "TypeRegistry.h"

namespace gau {

const char* TypeTagToString(TypeTag tag)
{
    switch (tag) {
    case TypeTag::Exec:
        return "exec";
    case TypeTag::Bool:
        return "bool";
    case TypeTag::Int:
        return "int";
    case TypeTag::Float:
        return "float";
    case TypeTag::String:
        return "string";
    case TypeTag::Object:
        return "object";
    case TypeTag::Enum:
        return "enum";
    case TypeTag::Struct:
        return "struct";
    case TypeTag::Array:
        return "array";
    case TypeTag::Set:
        return "set";
    case TypeTag::Map:
        return "map";
    }
    return "";
}

static int BuiltinSlot(TypeTag tag)
{
    switch (tag) {
    case TypeTag::Exec:
        return 0;
    case TypeTag::Bool:
        return 1;
    case TypeTag::Int:
        return 2;
    case TypeTag::Float:
        return 3;
    case TypeTag::String:
        return 4;
    case TypeTag::Object:
        return 5;
    default:
        return -1;
    }
}

TypeRegistry::TypeRegistry()
{
    Clear();
}

void TypeRegistry::Clear()
{
    descs.clear();
    enums.clear();
    structs.clear();
    // Index 0 is the invalid sentinel.
    descs.push_back(TypeDesc{});
    const TypeTag builtins[6] = {TypeTag::Exec,  TypeTag::Bool,   TypeTag::Int,
                                 TypeTag::Float, TypeTag::String, TypeTag::Object};
    for (int i = 0; i < 6; ++i) {
        TypeDesc desc;
        desc.tag = builtins[i];
        builtinIds[i] = Intern(desc);
    }
}

TypeId TypeRegistry::Builtin(TypeTag tag) const
{
    const int slot = BuiltinSlot(tag);
    return (slot >= 0) ? builtinIds[slot] : INVALID_TYPE;
}

static bool DescEquals(const TypeDesc& a, const TypeDesc& b)
{
    return a.tag == b.tag && a.name == b.name && a.element == b.element && a.key == b.key;
}

TypeId TypeRegistry::Intern(const TypeDesc& desc)
{
    for (std::size_t i = 1; i < descs.size(); ++i) {
        if (DescEquals(descs[i], desc)) {
            return static_cast<TypeId>(i);
        }
    }
    descs.push_back(desc);
    return static_cast<TypeId>(descs.size() - 1);
}

TypeId TypeRegistry::ArrayOf(TypeId element)
{
    TypeDesc desc;
    desc.tag = TypeTag::Array;
    desc.element = element;
    return Intern(desc);
}

TypeId TypeRegistry::SetOf(TypeId element)
{
    TypeDesc desc;
    desc.tag = TypeTag::Set;
    desc.element = element;
    return Intern(desc);
}

TypeId TypeRegistry::MapOf(TypeId key, TypeId value)
{
    TypeDesc desc;
    desc.tag = TypeTag::Map;
    desc.key = key;
    desc.element = value;
    return Intern(desc);
}

TypeId TypeRegistry::UserType(const std::string& name)
{
    TypeDesc desc;
    desc.name = name;
    if (FindStruct(name) != nullptr) {
        desc.tag = TypeTag::Struct;
    } else if (FindEnum(name) != nullptr) {
        desc.tag = TypeTag::Enum;
    } else {
        desc.tag = TypeTag::Object;
    }
    return Intern(desc);
}

const TypeDesc* TypeRegistry::Resolve(TypeId id) const
{
    if (id == INVALID_TYPE || id >= descs.size()) {
        return nullptr;
    }
    return &descs[id];
}

void TypeRegistry::DefineEnum(EnumDef def)
{
    for (EnumDef& existing : enums) {
        if (existing.name == def.name) {
            existing = std::move(def);
            return;
        }
    }
    enums.push_back(std::move(def));
}

void TypeRegistry::DefineStruct(StructDef def)
{
    for (StructDef& existing : structs) {
        if (existing.name == def.name) {
            existing = std::move(def);
            return;
        }
    }
    structs.push_back(std::move(def));
}

const EnumDef* TypeRegistry::FindEnum(const std::string& name) const
{
    for (const EnumDef& def : enums) {
        if (def.name == name) {
            return &def;
        }
    }
    return nullptr;
}

const StructDef* TypeRegistry::FindStruct(const std::string& name) const
{
    for (const StructDef& def : structs) {
        if (def.name == name) {
            return &def;
        }
    }
    return nullptr;
}

Value TypeRegistry::MakeDefault(TypeId id) const
{
    const TypeDesc* desc = Resolve(id);
    if (desc == nullptr) {
        return Value::None();
    }
    switch (desc->tag) {
    case TypeTag::Bool:
        return Value::Bool(false);
    case TypeTag::Int:
    case TypeTag::Enum:
        return Value::Int(0);
    case TypeTag::Float:
        return Value::Float(0.0);
    case TypeTag::String:
        return Value::Str(std::string());
    case TypeTag::Struct: {
        StructVal structVal;
        const StructDef* def = FindStruct(desc->name);
        if (def != nullptr) {
            for (const StructField& field : def->fields) {
                structVal.fields.push_back(MakeDefault(field.type));
            }
        }
        return Value(ValueData(std::move(structVal)));
    }
    case TypeTag::Array:
    case TypeTag::Set:
        return Value(ValueData(ArrayVal{}));
    case TypeTag::Map:
        return Value(ValueData(MapVal{}));
    case TypeTag::Exec:
    case TypeTag::Object:
        break;
    }
    return Value::None();
}

std::string TypeRegistry::TypeName(TypeId id) const
{
    const TypeDesc* desc = Resolve(id);
    if (desc == nullptr) {
        return "";
    }
    switch (desc->tag) {
    case TypeTag::Enum:
    case TypeTag::Struct:
    case TypeTag::Object:
        return desc->name;
    case TypeTag::Array:
        return "Array<" + TypeName(desc->element) + ">";
    case TypeTag::Set:
        return "Set<" + TypeName(desc->element) + ">";
    case TypeTag::Map:
        return "Map<" + TypeName(desc->key) + ", " + TypeName(desc->element) + ">";
    default:
        return TypeTagToString(desc->tag);
    }
}

} // namespace gau
