// v2 Project -> shared JSON.

#include "ProjectExport.h"

#include "core/TypeRegistry.h"

#include <nlohmann/json.hpp>

#include <variant>

namespace gau {

using nlohmann::json;

static json ValueToJson(const TypeRegistry& types, TypeId id, const Value& value)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return json();
    }
    switch (desc->tag) {
    case TypeTag::Bool:
        if (const bool* v = std::get_if<bool>(&value.data)) {
            return json(*v);
        }
        return json(false);
    case TypeTag::Int:
    case TypeTag::Enum:
        if (const std::int64_t* v = std::get_if<std::int64_t>(&value.data)) {
            return json(*v);
        }
        return json(0);
    case TypeTag::Float:
        if (const double* v = std::get_if<double>(&value.data)) {
            return json(*v);
        }
        return json(0.0);
    case TypeTag::String:
        if (const std::string* v = std::get_if<std::string>(&value.data)) {
            return json(*v);
        }
        return json("");
    case TypeTag::Struct: {
        json object = json::object();
        const StructVal* sv = std::get_if<StructVal>(&value.data);
        const StructDef* def = types.FindStruct(desc->name);
        if (sv != nullptr && def != nullptr) {
            for (std::size_t i = 0; i < def->fields.size() && i < sv->fields.size(); ++i) {
                object[def->fields[i].name] =
                    ValueToJson(types, def->fields[i].type, sv->fields[i]);
            }
        }
        return object;
    }
    case TypeTag::Array:
    case TypeTag::Set: {
        json array = json::array();
        if (const ArrayVal* av = std::get_if<ArrayVal>(&value.data)) {
            for (const Value& item : av->items) {
                array.push_back(ValueToJson(types, desc->element, item));
            }
        }
        return array;
    }
    case TypeTag::Map: {
        json object = json::object();
        if (const MapVal* mv = std::get_if<MapVal>(&value.data)) {
            for (const std::pair<Value, Value>& entry : mv->entries) {
                object[ValueToString(entry.first)] =
                    ValueToJson(types, desc->element, entry.second);
            }
        }
        return object;
    }
    default:
        return json();
    }
}

static void WritePropertyType(const TypeRegistry& types, TypeId id, json& out)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        out["type"] = "int";
        return;
    }
    switch (desc->tag) {
    case TypeTag::Array:
        out["container"] = "array";
        out["type"] = types.TypeName(desc->element);
        break;
    case TypeTag::Set:
        out["container"] = "set";
        out["type"] = types.TypeName(desc->element);
        break;
    case TypeTag::Map:
        out["container"] = "map";
        out["type"] = types.TypeName(desc->element);
        out["keyType"] = types.TypeName(desc->key);
        break;
    default:
        out["container"] = "none";
        out["type"] = types.TypeName(id);
        break;
    }
}

static json ExportTypes(const TypeRegistry& types)
{
    json array = json::array();
    for (const EnumDef& e : types.Enums()) {
        json entry;
        entry["name"] = e.name;
        entry["kind"] = "enum";
        entry["values"] = e.values;
        array.push_back(entry);
    }
    for (const StructDef& s : types.Structs()) {
        json entry;
        entry["name"] = s.name;
        entry["kind"] = "struct";
        json fields = json::array();
        for (const StructField& f : s.fields) {
            json field;
            field["name"] = f.name;
            field["type"] = types.TypeName(f.type);
            fields.push_back(field);
        }
        entry["fields"] = fields;
        array.push_back(entry);
    }
    return array;
}

static json ExportClasses(const TypeRegistry& types, const NodeClassRegistry& classes)
{
    json array = json::array();
    for (const NodeClass& c : classes.All()) {
        json entry;
        entry["name"] = c.name;
        entry["category"] = c.category;
        if (!c.execFn.empty()) {
            entry["execFn"] = c.execFn;
        }
        json pins = json::array();
        for (const PinDef& p : c.pins) {
            json pin;
            pin["direction"] = (p.direction == PinDirection::Output) ? "out" : "in";
            pin["type"] = types.TypeName(p.type);
            pin["name"] = p.name;
            pins.push_back(pin);
        }
        entry["pins"] = pins;
        json props = json::array();
        for (const PropertyDef& prop : c.properties) {
            json p;
            p["name"] = prop.name;
            WritePropertyType(types, prop.type, p);
            p["default"] = ValueToJson(types, prop.type, prop.defaultValue);
            props.push_back(p);
        }
        entry["properties"] = props;
        array.push_back(entry);
    }
    return array;
}

static int PinIndex(const std::vector<Pin>& pins, PinId id)
{
    for (int i = 0; i < static_cast<int>(pins.size()); ++i) {
        if (pins[static_cast<std::size_t>(i)].id == id) {
            return i;
        }
    }
    return -1;
}

static json ExportGraphJson(const Graph& graph, const TypeRegistry& types,
                            const NodeClassRegistry& classes)
{
    json nodes = json::array();
    for (const Node& node : graph.Nodes()) {
        json entry;
        entry["id"] = node.guid;
        entry["class"] = node.className;
        entry["x"] = node.x;
        entry["y"] = node.y;
        json props = json::array();
        const NodeClass* cls = classes.Find(node.className);
        for (std::size_t i = 0; i < node.properties.size(); ++i) {
            const TypeId propType =
                (cls != nullptr && i < cls->properties.size()) ? cls->properties[i].type
                                                               : INVALID_TYPE;
            props.push_back(ValueToJson(types, propType, node.properties[i]));
        }
        entry["properties"] = props;
        nodes.push_back(entry);
    }

    json links = json::array();
    for (const Link& link : graph.Links()) {
        const Node* fromNode = graph.FindPinOwner(link.fromPin);
        const Node* toNode = graph.FindPinOwner(link.toPin);
        if (fromNode == nullptr || toNode == nullptr) {
            continue;
        }
        json entry;
        entry["fromId"] = fromNode->guid;
        entry["fromPin"] = PinIndex(fromNode->outputs, link.fromPin);
        entry["toId"] = toNode->guid;
        entry["toPin"] = PinIndex(toNode->inputs, link.toPin);
        links.push_back(entry);
    }

    json root;
    root["nodes"] = nodes;
    root["links"] = links;
    return root;
}

static json ExportFunctions(const Project& project)
{
    json array = json::array();
    for (const auto& defPtr : project.functions.All()) {
        const FunctionDef& def = *defPtr;
        json entry;
        entry["name"] = def.name;
        entry["hasExec"] = def.hasExec;
        json inputs = json::array();
        for (const FunctionParam& p : def.inputs) {
            json param;
            param["name"] = p.name;
            param["type"] = project.types.TypeName(p.type);
            inputs.push_back(param);
        }
        entry["inputs"] = inputs;
        json outputs = json::array();
        for (const FunctionParam& p : def.outputs) {
            json param;
            param["name"] = p.name;
            param["type"] = project.types.TypeName(p.type);
            outputs.push_back(param);
        }
        entry["outputs"] = outputs;
        entry["body"] = ExportGraphJson(*def.body, project.types, project.classes);
        array.push_back(entry);
    }
    return array;
}

static json ExportVariables(const Project& project)
{
    json array = json::array();
    for (const VariableDef& v : project.variables) {
        json entry;
        entry["name"] = v.name;
        entry["type"] = project.types.TypeName(v.type);
        array.push_back(entry);
    }
    return array;
}

std::string ExportProject(const Project& project)
{
    json root;
    root["schemaVersion"] = PROJECT_SCHEMA_VERSION;
    root["types"] = ExportTypes(project.types);
    root["nodeClasses"] = ExportClasses(project.types, project.classes);
    root["functions"] = ExportFunctions(project);
    root["variables"] = ExportVariables(project);
    const json graphJson = ExportGraphJson(*project.graph, project.types, project.classes);
    root["nodes"] = graphJson["nodes"];
    root["links"] = graphJson["links"];
    return root.dump(2);
}

} // namespace gau
