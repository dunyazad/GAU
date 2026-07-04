// v1 -> v2 JSON migration.

#include "V1Import.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <cstdlib>
#include <string>
#include <unordered_map>

namespace gau {

using nlohmann::json;

static std::string ToLower(const std::string& text)
{
    std::string out = text;
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}

static TypeId ResolveType(TypeRegistry& types, const std::string& name)
{
    const std::string lower = ToLower(name);
    if (lower == "exec") {
        return types.Builtin(TypeTag::Exec);
    }
    if (lower == "bool") {
        return types.Builtin(TypeTag::Bool);
    }
    if (lower == "int") {
        return types.Builtin(TypeTag::Int);
    }
    if (lower == "float") {
        return types.Builtin(TypeTag::Float);
    }
    if (lower == "string") {
        return types.Builtin(TypeTag::String);
    }
    if (lower == "object") {
        return types.Builtin(TypeTag::Object);
    }
    return types.UserType(name);
}

static TypeId ResolvePropertyType(TypeRegistry& types, const std::string& container,
                                  const std::string& type, const std::string& keyType)
{
    const TypeId element = ResolveType(types, type);
    const std::string c = ToLower(container);
    if (c == "array") {
        return types.ArrayOf(element);
    }
    if (c == "set") {
        return types.SetOf(element);
    }
    if (c == "map") {
        return types.MapOf(ResolveType(types, keyType), element);
    }
    return element;
}

static Value ParseScalarString(const TypeRegistry& types, TypeId id, const std::string& text)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return Value::None();
    }
    switch (desc->tag) {
    case TypeTag::Bool:
        return Value::Bool(text == "true" || text == "1");
    case TypeTag::Int:
    case TypeTag::Enum:
        return Value::Int(std::strtoll(text.c_str(), nullptr, 10));
    case TypeTag::Float:
        return Value::Float(std::strtod(text.c_str(), nullptr));
    case TypeTag::String:
        return Value::Str(text);
    default:
        return Value::None();
    }
}

static Value JsonToValue(const TypeRegistry& types, TypeId id, const json& j)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return Value::None();
    }
    switch (desc->tag) {
    case TypeTag::Bool:
        return j.is_boolean() ? Value::Bool(j.get<bool>()) : types.MakeDefault(id);
    case TypeTag::Int:
    case TypeTag::Enum:
        return j.is_number_integer() ? Value::Int(j.get<std::int64_t>()) : types.MakeDefault(id);
    case TypeTag::Float:
        return j.is_number() ? Value::Float(j.get<double>()) : types.MakeDefault(id);
    case TypeTag::String:
        return j.is_string() ? Value::Str(j.get<std::string>()) : types.MakeDefault(id);
    case TypeTag::Struct: {
        if (!j.is_object()) {
            return types.MakeDefault(id);
        }
        StructVal sv;
        const StructDef* def = types.FindStruct(desc->name);
        if (def != nullptr) {
            for (const StructField& field : def->fields) {
                if (j.contains(field.name)) {
                    sv.fields.push_back(JsonToValue(types, field.type, j[field.name]));
                } else {
                    sv.fields.push_back(types.MakeDefault(field.type));
                }
            }
        }
        return Value(ValueData(std::move(sv)));
    }
    case TypeTag::Array:
    case TypeTag::Set: {
        if (!j.is_array()) {
            return types.MakeDefault(id);
        }
        ArrayVal av;
        for (const json& item : j) {
            av.items.push_back(JsonToValue(types, desc->element, item));
        }
        return Value(ValueData(std::move(av)));
    }
    case TypeTag::Map: {
        if (!j.is_object()) {
            return types.MakeDefault(id);
        }
        MapVal mv;
        for (const auto& item : j.items()) {
            mv.entries.emplace_back(ParseScalarString(types, desc->key, item.key()),
                                    JsonToValue(types, desc->element, item.value()));
        }
        return Value(ValueData(std::move(mv)));
    }
    default:
        return Value::None();
    }
}

static PinDirection ParseDirection(const std::string& text)
{
    const std::string lower = ToLower(text);
    return (lower == "out" || lower == "output") ? PinDirection::Output : PinDirection::Input;
}

static void ImportTypes(const json& typesJson, TypeRegistry& types, ImportCounts& counts)
{
    for (const json& t : typesJson) {
        if (!t.is_object() || !t.contains("name") || !t["name"].is_string()) {
            continue;
        }
        const std::string name = t["name"].get<std::string>();
        const std::string kind = t.contains("kind") && t["kind"].is_string()
                                     ? ToLower(t["kind"].get<std::string>())
                                     : "enum";
        if (kind == "struct") {
            StructDef def;
            def.name = name;
            if (t.contains("fields") && t["fields"].is_array()) {
                for (const json& f : t["fields"]) {
                    if (f.is_object() && f.contains("name") && f.contains("type")) {
                        StructField field;
                        field.name = f["name"].get<std::string>();
                        field.type = ResolveType(types, f["type"].get<std::string>());
                        def.fields.push_back(std::move(field));
                    }
                }
            }
            types.DefineStruct(std::move(def));
        } else if (kind == "enum") {
            EnumDef def;
            def.name = name;
            if (t.contains("values") && t["values"].is_array()) {
                for (const json& v : t["values"]) {
                    if (v.is_string()) {
                        def.values.push_back(v.get<std::string>());
                    }
                }
            }
            types.DefineEnum(std::move(def));
        }
        // object alias: no definition needed; UserType() yields Object.
        ++counts.types;
    }
}

static void ImportClasses(const json& classesJson, TypeRegistry& types,
                          NodeClassRegistry& classes, ImportCounts& counts)
{
    for (const json& c : classesJson) {
        if (!c.is_object() || !c.contains("name") || !c["name"].is_string()) {
            continue;
        }
        NodeClass nodeClass;
        nodeClass.name = c["name"].get<std::string>();
        nodeClass.category = c.contains("category") && c["category"].is_string()
                                 ? c["category"].get<std::string>()
                                 : "Function";
        if (c.contains("execFn") && c["execFn"].is_string()) {
            nodeClass.execFn = c["execFn"].get<std::string>();
        }
        if (c.contains("pins") && c["pins"].is_array()) {
            for (const json& p : c["pins"]) {
                if (!p.is_object() || !p.contains("type") || !p.contains("direction")) {
                    continue;
                }
                PinDef pin;
                pin.direction = ParseDirection(p["direction"].get<std::string>());
                pin.type = ResolveType(types, p["type"].get<std::string>());
                pin.name = p.contains("name") && p["name"].is_string()
                               ? p["name"].get<std::string>()
                               : "";
                nodeClass.pins.push_back(std::move(pin));
            }
        }
        if (c.contains("properties") && c["properties"].is_array()) {
            for (const json& p : c["properties"]) {
                if (!p.is_object() || !p.contains("name") || !p.contains("type")) {
                    continue;
                }
                PropertyDef prop;
                prop.name = p["name"].get<std::string>();
                const std::string container = p.contains("container") && p["container"].is_string()
                                                  ? p["container"].get<std::string>()
                                                  : "none";
                const std::string keyType = p.contains("keyType") && p["keyType"].is_string()
                                                ? p["keyType"].get<std::string>()
                                                : "string";
                prop.type = ResolvePropertyType(types, container, p["type"].get<std::string>(),
                                                keyType);
                if (p.contains("default")) {
                    prop.defaultValue = JsonToValue(types, prop.type, p["default"]);
                } else {
                    prop.defaultValue = types.MakeDefault(prop.type);
                }
                nodeClass.properties.push_back(std::move(prop));
            }
        }
        classes.Register(std::move(nodeClass));
        ++counts.classes;
    }
}

ImportCounts ImportV1Definitions(const std::string& jsonText, TypeRegistry& types,
                                 NodeClassRegistry& classes, std::vector<std::string>& errors)
{
    ImportCounts counts;
    const json root = json::parse(jsonText, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        errors.push_back("invalid JSON");
        return counts;
    }
    if (root.contains("types") && root["types"].is_array()) {
        ImportTypes(root["types"], types, counts);
    }
    if (root.contains("nodeClasses") && root["nodeClasses"].is_array()) {
        ImportClasses(root["nodeClasses"], types, classes, counts);
    }
    return counts;
}

bool ImportV1Graph(const std::string& jsonText, Graph& graph, const NodeClassRegistry& classes,
                   TypeRegistry& types, std::vector<std::string>& errors)
{
    const json root = json::parse(jsonText, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        errors.push_back("invalid JSON");
        return false;
    }

    std::unordered_map<std::string, NodeId> guidToNode;
    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const json& n : root["nodes"]) {
            if (!n.is_object() || !n.contains("class") || !n["class"].is_string()) {
                continue;
            }
            const std::string className = n["class"].get<std::string>();
            const NodeClass* cls = classes.Find(className);
            if (cls == nullptr) {
                errors.push_back("unknown class: " + className);
                continue;
            }
            const float x = n.contains("x") ? n["x"].get<float>() : 0.0f;
            const float y = n.contains("y") ? n["y"].get<float>() : 0.0f;
            const NodeId id = graph.AddNode(*cls, x, y);
            if (n.contains("id") && n["id"].is_string()) {
                guidToNode[n["id"].get<std::string>()] = id;
            }
            if (n.contains("properties") && n["properties"].is_array()) {
                Node* node = graph.FindNode(id);
                const json& props = n["properties"];
                for (std::size_t i = 0; i < cls->properties.size() && i < props.size()
                                        && i < node->properties.size();
                     ++i) {
                    node->properties[i] = JsonToValue(types, cls->properties[i].type, props[i]);
                }
            }
        }
    }

    if (root.contains("links") && root["links"].is_array()) {
        for (const json& l : root["links"]) {
            if (!l.is_object() || !l.contains("fromId") || !l.contains("toId")) {
                continue;
            }
            auto fromIt = guidToNode.find(l["fromId"].get<std::string>());
            auto toIt = guidToNode.find(l["toId"].get<std::string>());
            if (fromIt == guidToNode.end() || toIt == guidToNode.end()) {
                continue;
            }
            const int fromPin = l.contains("fromPin") ? l["fromPin"].get<int>() : -1;
            const int toPin = l.contains("toPin") ? l["toPin"].get<int>() : -1;
            const Node* fromNode = graph.FindNode(fromIt->second);
            const Node* toNode = graph.FindNode(toIt->second);
            if (fromNode == nullptr || toNode == nullptr || fromPin < 0 || toPin < 0
                || fromPin >= static_cast<int>(fromNode->outputs.size())
                || toPin >= static_cast<int>(toNode->inputs.size())) {
                continue;
            }
            graph.AddLink(fromNode->outputs[static_cast<std::size_t>(fromPin)].id,
                          toNode->inputs[static_cast<std::size_t>(toPin)].id);
        }
    }
    return true;
}

} // namespace gau
