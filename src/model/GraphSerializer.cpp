#include "GraphSerializer.h"
#include "NodeClass.h"
#include "NodeGraph.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <utility>

using nlohmann::json;

static json ValueToJson(const Value& value)
{
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        return json(*boolValue);
    }
    if (const int* intValue = std::get_if<int>(&value)) {
        return json(*intValue);
    }
    if (const double* doubleValue = std::get_if<double>(&value)) {
        return json(*doubleValue);
    }
    if (const std::string* stringValue = std::get_if<std::string>(&value)) {
        return json(*stringValue);
    }
    return json();
}

static bool JsonToValue(const json& valueJson, PinType type, Value& outValue)
{
    switch (type) {
    case PinType::Bool:
        if (!valueJson.is_boolean()) {
            return false;
        }
        outValue = Value(valueJson.get<bool>());
        return true;
    case PinType::Int:
        if (!valueJson.is_number_integer()) {
            return false;
        }
        outValue = Value(valueJson.get<int>());
        return true;
    case PinType::Float:
        if (!valueJson.is_number()) {
            return false;
        }
        outValue = Value(valueJson.get<double>());
        return true;
    case PinType::String:
        if (!valueJson.is_string()) {
            return false;
        }
        outValue = Value(valueJson.get<std::string>());
        return true;
    case PinType::Exec:
    case PinType::Object:
        break;
    }
    return false;
}

static json PropertyValueToJson(const PropertyDef& def, const PropertyValue& value)
{
    switch (def.container) {
    case PropertyContainer::None:
        return ValueToJson(value.scalar);
    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        json array = json::array();
        for (const Value& element : value.elements) {
            array.push_back(ValueToJson(element));
        }
        return array;
    }
    case PropertyContainer::Map: {
        json object = json::object();
        for (const std::pair<Value, Value>& entry : value.entries) {
            object[ValueToString(entry.first)] = ValueToJson(entry.second);
        }
        return object;
    }
    }
    return json();
}

// Parses one serialized property value according to the class property
// definition; falls back to the class default on mismatch.
static bool JsonToPropertyValue(const json& valueJson, const PropertyDef& def,
                                PropertyValue& outValue)
{
    switch (def.container) {
    case PropertyContainer::None:
        return JsonToValue(valueJson, def.type, outValue.scalar);

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        if (!valueJson.is_array()) {
            return false;
        }
        outValue.elements.clear();
        for (const json& elementJson : valueJson) {
            Value element;
            if (!JsonToValue(elementJson, def.type, element)) {
                return false;
            }
            outValue.elements.push_back(std::move(element));
        }
        return true;
    }

    case PropertyContainer::Map: {
        if (!valueJson.is_object()) {
            return false;
        }
        outValue.entries.clear();
        for (const auto& item : valueJson.items()) {
            Value key;
            if (!ParseValueString(item.key(), def.keyType, key)) {
                return false;
            }
            Value value;
            if (!JsonToValue(item.value(), def.type, value)) {
                return false;
            }
            outValue.entries.emplace_back(std::move(key), std::move(value));
        }
        return true;
    }
    }
    return false;
}

bool SaveGraphToFile(const NodeGraph& graph, const std::string& path, std::string& outError)
{
    json root;

    json nodeArray = json::array();
    for (const Node& node : graph.GetNodes()) {
        json entry;
        entry["class"] = node.nodeClass->GetName();
        entry["x"] = node.x;
        entry["y"] = node.y;

        const std::vector<PropertyDef>& defs = node.nodeClass->GetProperties();
        json propertyArray = json::array();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            PropertyValue fallback;
            fallback.scalar = defs[i].defaultValue;
            fallback.elements = defs[i].defaultElements;
            fallback.entries = defs[i].defaultEntries;
            const PropertyValue& value =
                (i < node.propertyValues.size()) ? node.propertyValues[i] : fallback;
            propertyArray.push_back(PropertyValueToJson(defs[i], value));
        }
        entry["properties"] = propertyArray;
        nodeArray.push_back(entry);
    }
    root["nodes"] = nodeArray;

    json commentArray = json::array();
    for (const CommentNode& comment : graph.GetComments()) {
        json entry;
        entry["title"] = comment.title;
        entry["x"] = comment.x;
        entry["y"] = comment.y;
        entry["width"] = comment.width;
        entry["height"] = comment.height;
        commentArray.push_back(entry);
    }
    root["comments"] = commentArray;

    std::ofstream file(path);
    if (!file.is_open()) {
        outError = "cannot open file for writing: " + path;
        return false;
    }
    file << root.dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    if (!file.good()) {
        outError = "write failed: " + path;
        return false;
    }
    return true;
}

static void LoadNodeEntry(NodeGraph& graph, const json& entry,
                          std::vector<std::string>& outErrors)
{
    if (!entry.is_object() || !entry.contains("class") || !entry["class"].is_string()) {
        outErrors.push_back("node entry has missing or invalid 'class'");
        return;
    }
    const std::string className = entry["class"].get<std::string>();
    const NodeClass* nodeClass = NodeClass::FindByName(className.c_str());
    if (nodeClass == nullptr) {
        outErrors.push_back("unknown node class: " + className);
        return;
    }

    const float x = (entry.contains("x") && entry["x"].is_number())
                        ? entry["x"].get<float>() : 0.0f;
    const float y = (entry.contains("y") && entry["y"].is_number())
                        ? entry["y"].get<float>() : 0.0f;

    const NodeId nodeId = graph.AddNode(*nodeClass, x, y);
    Node* node = graph.FindNode(nodeId);
    if (node == nullptr) {
        return;
    }

    if (!entry.contains("properties") || !entry["properties"].is_array()) {
        return;
    }
    const json& propertyArray = entry["properties"];
    const std::vector<PropertyDef>& defs = nodeClass->GetProperties();
    for (std::size_t i = 0; i < defs.size() && i < propertyArray.size()
                            && i < node->propertyValues.size(); ++i) {
        PropertyValue value = node->propertyValues[i];
        if (JsonToPropertyValue(propertyArray[i], defs[i], value)) {
            node->propertyValues[i] = std::move(value);
        } else {
            outErrors.push_back("node '" + className + "': property '" + defs[i].name
                                + "' has a mismatched value; default kept");
        }
    }
}

bool LoadGraphFromFile(NodeGraph& graph, const std::string& path,
                       std::vector<std::string>& outErrors)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        outErrors.push_back("cannot open file: " + path);
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    const json root = json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        outErrors.push_back("invalid JSON: " + path);
        return false;
    }

    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const json& entry : root["nodes"]) {
            LoadNodeEntry(graph, entry, outErrors);
        }
    }

    if (root.contains("comments") && root["comments"].is_array()) {
        for (const json& entry : root["comments"]) {
            if (!entry.is_object()) {
                continue;
            }
            const std::string title = (entry.contains("title") && entry["title"].is_string())
                                          ? entry["title"].get<std::string>() : "Comment";
            const float x = (entry.contains("x") && entry["x"].is_number())
                                ? entry["x"].get<float>() : 0.0f;
            const float y = (entry.contains("y") && entry["y"].is_number())
                                ? entry["y"].get<float>() : 0.0f;
            const float width = (entry.contains("width") && entry["width"].is_number())
                                    ? entry["width"].get<float>() : COMMENT_DEFAULT_WIDTH;
            const float height = (entry.contains("height") && entry["height"].is_number())
                                     ? entry["height"].get<float>() : COMMENT_DEFAULT_HEIGHT;
            graph.AddComment(title, x, y, width, height);
        }
    }
    return true;
}
