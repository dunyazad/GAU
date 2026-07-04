#include "GraphSerializer.h"
#include "NodeClass.h"
#include "NodeGraph.h"
#include "PropertyText.h"
#include "UserType.h"

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

// A struct property value serializes as a JSON object keyed by field name;
// fields may nest structs recursively.
static json StructValueToJson(const UserType& structType,
                              const std::vector<PropertyValue>& fields)
{
    json object = json::object();
    for (std::size_t i = 0; i < structType.fields.size(); ++i) {
        const StructField& fieldDef = structType.fields[i];
        const PropertyValue empty;
        const PropertyValue& fieldValue = (i < fields.size()) ? fields[i] : empty;
        const UserType* nested = (fieldDef.type == PinType::UserType)
                                     ? UserTypeRegistry::Find(fieldDef.typeName)
                                     : nullptr;
        if (nested != nullptr && nested->kind == UserTypeKind::Struct) {
            object[fieldDef.name] = StructValueToJson(*nested, fieldValue.structFields);
        } else {
            object[fieldDef.name] = ValueToJson(fieldValue.scalar);
        }
    }
    return object;
}

// True when a scalar (container None) property holds a user struct value.
static const UserType* StructTypeOfProperty(const PropertyDef& def)
{
    if (def.container != PropertyContainer::None || def.type != PinType::UserType) {
        return nullptr;
    }
    const UserType* userType = UserTypeRegistry::Find(def.typeName);
    if (userType != nullptr && userType->kind == UserTypeKind::Struct) {
        return userType;
    }
    return nullptr;
}

static json PropertyValueToJson(const PropertyDef& def, const PropertyValue& value)
{
    switch (def.container) {
    case PropertyContainer::None:
        if (const UserType* structType = StructTypeOfProperty(def)) {
            return StructValueToJson(*structType, value.structFields);
        }
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

static bool JsonToStructValue(const UserType& structType, const json& valueJson,
                              std::vector<PropertyValue>& outFields)
{
    if (!valueJson.is_object()) {
        return false;
    }
    outFields.clear();
    for (const StructField& fieldDef : structType.fields) {
        PropertyValue fieldValue;
        const bool present = valueJson.contains(fieldDef.name);
        const UserType* nested = (fieldDef.type == PinType::UserType)
                                     ? UserTypeRegistry::Find(fieldDef.typeName)
                                     : nullptr;
        if (nested != nullptr && nested->kind == UserTypeKind::Struct) {
            if (present) {
                JsonToStructValue(*nested, valueJson[fieldDef.name], fieldValue.structFields);
            }
        } else if (present) {
            JsonToValue(valueJson[fieldDef.name], fieldDef.type, fieldValue.scalar);
        } else {
            fieldValue.scalar = MakeDefaultValue(fieldDef.type);
        }
        outFields.push_back(std::move(fieldValue));
    }
    return true;
}

// Parses one serialized property value according to the class property
// definition; falls back to the class default on mismatch.
static bool JsonToPropertyValue(const json& valueJson, const PropertyDef& def,
                                PropertyValue& outValue)
{
    switch (def.container) {
    case PropertyContainer::None:
        if (const UserType* structType = StructTypeOfProperty(def)) {
            return JsonToStructValue(*structType, valueJson, outValue.structFields);
        }
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
        entry["id"] = node.guid;
        entry["class"] = node.nodeClass->GetName();
        entry["x"] = node.x;
        entry["y"] = node.y;

        const std::vector<PropertyDef>& defs = node.nodeClass->GetProperties();
        json propertyArray = json::array();
        for (std::size_t i = 0; i < defs.size(); ++i) {
            const PropertyValue fallback = MakeDefaultPropertyValue(defs[i]);
            const PropertyValue& value =
                (i < node.propertyValues.size()) ? node.propertyValues[i] : fallback;
            propertyArray.push_back(PropertyValueToJson(defs[i], value));
        }
        entry["properties"] = propertyArray;
        nodeArray.push_back(entry);
    }
    root["nodes"] = nodeArray;

    // Links reference nodes by guid and pins by their position within
    // the node's output/input lists (guids stay stable across sessions
    // and merges, unlike array indices).
    json linkArray = json::array();
    for (const Link& link : graph.GetLinks()) {
        std::string fromGuid;
        int fromPinIndex = -1;
        std::string toGuid;
        int toPinIndex = -1;
        for (const Node& node : graph.GetNodes()) {
            for (int pinIndex = 0; pinIndex < static_cast<int>(node.outputs.size()); ++pinIndex) {
                if (node.outputs[static_cast<std::size_t>(pinIndex)].id == link.fromPinId) {
                    fromGuid = node.guid;
                    fromPinIndex = pinIndex;
                }
            }
            for (int pinIndex = 0; pinIndex < static_cast<int>(node.inputs.size()); ++pinIndex) {
                if (node.inputs[static_cast<std::size_t>(pinIndex)].id == link.toPinId) {
                    toGuid = node.guid;
                    toPinIndex = pinIndex;
                }
            }
        }
        if (fromGuid.empty() || toGuid.empty()) {
            continue;
        }
        json entry;
        entry["fromId"] = fromGuid;
        entry["fromPin"] = fromPinIndex;
        entry["toId"] = toGuid;
        entry["toPin"] = toPinIndex;
        json pointArray = json::array();
        for (const LinkPoint& point : link.points) {
            json pointJson;
            pointJson["x"] = point.x;
            pointJson["y"] = point.y;
            pointArray.push_back(pointJson);
        }
        entry["points"] = pointArray;
        linkArray.push_back(entry);
    }
    root["links"] = linkArray;

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

// Returns the created node id, or INVALID_ID when the entry was skipped
// (so link indices can be mapped correctly).
static NodeId LoadNodeEntry(NodeGraph& graph, const json& entry,
                            std::vector<std::string>& outErrors)
{
    if (!entry.is_object() || !entry.contains("class") || !entry["class"].is_string()) {
        outErrors.push_back("node entry has missing or invalid 'class'");
        return INVALID_ID;
    }
    const std::string className = entry["class"].get<std::string>();
    const NodeClass* nodeClass = NodeClass::FindByName(className.c_str());
    if (nodeClass == nullptr) {
        outErrors.push_back("unknown node class: " + className);
        return INVALID_ID;
    }

    const float x = (entry.contains("x") && entry["x"].is_number())
                        ? entry["x"].get<float>() : 0.0f;
    const float y = (entry.contains("y") && entry["y"].is_number())
                        ? entry["y"].get<float>() : 0.0f;

    const NodeId nodeId = graph.AddNode(*nodeClass, x, y);
    Node* node = graph.FindNode(nodeId);
    if (node == nullptr) {
        return INVALID_ID;
    }

    // Restore the persistent guid when present and not already taken
    // (malformed duplicates keep the freshly generated one).
    if (entry.contains("id") && entry["id"].is_string()) {
        const std::string fileGuid = entry["id"].get<std::string>();
        if (!fileGuid.empty()) {
            const Node* existing = graph.FindNodeByGuid(fileGuid);
            if (existing == nullptr || existing == node) {
                node->guid = fileGuid;
            } else {
                outErrors.push_back("duplicate node guid in file: " + fileGuid);
            }
        }
    }

    if (!entry.contains("properties") || !entry["properties"].is_array()) {
        return nodeId;
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
    return nodeId;
}

// Resolves the endpoint nodes of a link entry: new files use node guids
// ("fromId"/"toId"), older files array indices ("fromNode"/"toNode").
static bool ResolveLinkNodes(const NodeGraph& graph, const json& entry,
                             const std::vector<NodeId>& loadedNodeIds,
                             const Node*& outFromNode, const Node*& outToNode,
                             std::vector<std::string>& outErrors)
{
    if (entry.contains("fromId") && entry["fromId"].is_string()
        && entry.contains("toId") && entry["toId"].is_string()) {
        outFromNode = graph.FindNodeByGuid(entry["fromId"].get<std::string>());
        outToNode = graph.FindNodeByGuid(entry["toId"].get<std::string>());
        if (outFromNode == nullptr || outToNode == nullptr) {
            outErrors.push_back("link references an unknown node guid");
            return false;
        }
        return true;
    }

    if (entry.contains("fromNode") && entry["fromNode"].is_number_integer()
        && entry.contains("toNode") && entry["toNode"].is_number_integer()) {
        const int fromNodeIndex = entry["fromNode"].get<int>();
        const int toNodeIndex = entry["toNode"].get<int>();
        if (fromNodeIndex < 0 || fromNodeIndex >= static_cast<int>(loadedNodeIds.size())
            || toNodeIndex < 0 || toNodeIndex >= static_cast<int>(loadedNodeIds.size())) {
            outErrors.push_back("link references a node out of range");
            return false;
        }
        const NodeId fromNodeId = loadedNodeIds[static_cast<std::size_t>(fromNodeIndex)];
        const NodeId toNodeId = loadedNodeIds[static_cast<std::size_t>(toNodeIndex)];
        if (fromNodeId == INVALID_ID || toNodeId == INVALID_ID) {
            outErrors.push_back("link references a skipped node");
            return false;
        }
        outFromNode = graph.FindNode(fromNodeId);
        outToNode = graph.FindNode(toNodeId);
        return outFromNode != nullptr && outToNode != nullptr;
    }

    outErrors.push_back("link entry is malformed");
    return false;
}

static void LoadLinkEntry(NodeGraph& graph, const json& entry,
                          const std::vector<NodeId>& loadedNodeIds,
                          std::vector<std::string>& outErrors)
{
    if (!entry.is_object()
        || !entry.contains("fromPin") || !entry["fromPin"].is_number_integer()
        || !entry.contains("toPin") || !entry["toPin"].is_number_integer()) {
        outErrors.push_back("link entry is malformed");
        return;
    }
    const int fromPinIndex = entry["fromPin"].get<int>();
    const int toPinIndex = entry["toPin"].get<int>();

    const Node* fromNode = nullptr;
    const Node* toNode = nullptr;
    if (!ResolveLinkNodes(graph, entry, loadedNodeIds, fromNode, toNode, outErrors)) {
        return;
    }

    if (fromNode == nullptr || toNode == nullptr
        || fromPinIndex < 0 || fromPinIndex >= static_cast<int>(fromNode->outputs.size())
        || toPinIndex < 0 || toPinIndex >= static_cast<int>(toNode->inputs.size())) {
        outErrors.push_back("link references a pin out of range");
        return;
    }

    const Pin& outputPinData = fromNode->outputs[static_cast<std::size_t>(fromPinIndex)];
    const PinId outputPin = outputPinData.id;
    const PinId inputPin = toNode->inputs[static_cast<std::size_t>(toPinIndex)].id;
    const bool exclusiveSideOccupied =
        (outputPinData.type == PinType::Exec)
            ? graph.FindLinkFromOutput(outputPin) != nullptr
            : graph.FindLinkToInput(inputPin) != nullptr;
    if (!graph.CanConnect(outputPin, inputPin) || exclusiveSideOccupied) {
        outErrors.push_back("link skipped: connection is no longer valid");
        return;
    }
    const LinkId linkId = graph.AddLink(outputPin, inputPin);

    if (entry.contains("points") && entry["points"].is_array()) {
        Link* link = graph.FindLink(linkId);
        if (link != nullptr) {
            for (const json& pointJson : entry["points"]) {
                if (pointJson.is_object()
                    && pointJson.contains("x") && pointJson["x"].is_number()
                    && pointJson.contains("y") && pointJson["y"].is_number()) {
                    LinkPoint point;
                    point.x = pointJson["x"].get<float>();
                    point.y = pointJson["y"].get<float>();
                    link->points.push_back(point);
                }
            }
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

    std::vector<NodeId> loadedNodeIds;
    if (root.contains("nodes") && root["nodes"].is_array()) {
        for (const json& entry : root["nodes"]) {
            loadedNodeIds.push_back(LoadNodeEntry(graph, entry, outErrors));
        }
    }

    if (root.contains("links") && root["links"].is_array()) {
        for (const json& entry : root["links"]) {
            LoadLinkEntry(graph, entry, loadedNodeIds, outErrors);
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
