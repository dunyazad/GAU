#include "NodeClassLoader.h"
#include "NodeClass.h"

#include <nlohmann/json.hpp>

#include <cctype>
#include <fstream>
#include <memory>
#include <sstream>
#include <utility>

using nlohmann::json;

static std::string ToLowerAscii(const std::string& text)
{
    std::string result = text;
    for (char& c : result) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return result;
}

static bool ParsePinDirection(const std::string& text, PinDirection& outDirection)
{
    const std::string lower = ToLowerAscii(text);
    if (lower == "in" || lower == "input") {
        outDirection = PinDirection::Input;
    } else if (lower == "out" || lower == "output") {
        outDirection = PinDirection::Output;
    } else {
        return false;
    }
    return true;
}

static bool ParsePinType(const std::string& text, PinType& outType)
{
    const std::string lower = ToLowerAscii(text);
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
    } else {
        return false;
    }
    return true;
}

static bool ParsePin(const json& pinJson, PinDef& outPin, std::string& outError)
{
    if (!pinJson.is_object()) {
        outError = "pin entry is not an object";
        return false;
    }
    if (!pinJson.contains("direction") || !pinJson["direction"].is_string()
        || !ParsePinDirection(pinJson["direction"].get<std::string>(), outPin.direction)) {
        outError = "pin has missing or invalid 'direction'";
        return false;
    }
    if (!pinJson.contains("type") || !pinJson["type"].is_string()
        || !ParsePinType(pinJson["type"].get<std::string>(), outPin.type)) {
        outError = "pin has missing or invalid 'type'";
        return false;
    }
    outPin.name.clear();
    if (pinJson.contains("name") && pinJson["name"].is_string()) {
        outPin.name = pinJson["name"].get<std::string>();
    }
    return true;
}

static bool ParsePropertyDefault(const json& defaultJson, PinType type, Value& outValue,
                                 std::string& outError)
{
    switch (type) {
    case PinType::Bool:
        if (!defaultJson.is_boolean()) {
            outError = "default must be a boolean";
            return false;
        }
        outValue = Value(defaultJson.get<bool>());
        return true;
    case PinType::Int:
        if (!defaultJson.is_number_integer()) {
            outError = "default must be an integer";
            return false;
        }
        outValue = Value(defaultJson.get<int>());
        return true;
    case PinType::Float:
        if (!defaultJson.is_number()) {
            outError = "default must be a number";
            return false;
        }
        outValue = Value(defaultJson.get<double>());
        return true;
    case PinType::String:
        if (!defaultJson.is_string()) {
            outError = "default must be a string";
            return false;
        }
        outValue = Value(defaultJson.get<std::string>());
        return true;
    case PinType::Exec:
    case PinType::Object:
        break;
    }
    outError = "unsupported property type";
    return false;
}

static bool ValueEquals(const Value& a, const Value& b)
{
    return a == b;
}

static bool ParseContainerDefault(const json& defaultJson, PropertyDef& property,
                                  std::string& outError)
{
    switch (property.container) {
    case PropertyContainer::None:
        return ParsePropertyDefault(defaultJson, property.type, property.defaultValue, outError);

    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        if (!defaultJson.is_array()) {
            outError = "default must be an array";
            return false;
        }
        for (const json& elementJson : defaultJson) {
            Value element;
            if (!ParsePropertyDefault(elementJson, property.type, element, outError)) {
                return false;
            }
            if (property.container == PropertyContainer::Set) {
                bool duplicate = false;
                for (const Value& existing : property.defaultElements) {
                    if (ValueEquals(existing, element)) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) {
                    continue;
                }
            }
            property.defaultElements.push_back(std::move(element));
        }
        return true;
    }

    case PropertyContainer::Map: {
        if (!defaultJson.is_object()) {
            outError = "default must be an object (key -> value)";
            return false;
        }
        for (const auto& item : defaultJson.items()) {
            Value key;
            if (!ParseValueString(item.key(), property.keyType, key)) {
                outError = "map key '" + item.key() + "' does not match key type";
                return false;
            }
            for (const std::pair<Value, Value>& existing : property.defaultEntries) {
                if (ValueEquals(existing.first, key)) {
                    outError = "duplicate map key: " + item.key();
                    return false;
                }
            }
            Value value;
            if (!ParsePropertyDefault(item.value(), property.type, value, outError)) {
                return false;
            }
            property.defaultEntries.emplace_back(std::move(key), std::move(value));
        }
        return true;
    }
    }
    outError = "unsupported container";
    return false;
}

static bool ParsePropertyValueType(const json& parent, const char* field, PinType& outType,
                                   std::string& outError)
{
    if (!parent.contains(field) || !parent[field].is_string()
        || !ParsePinType(parent[field].get<std::string>(), outType)) {
        outError = std::string("missing or invalid '") + field + "'";
        return false;
    }
    if (outType == PinType::Exec || outType == PinType::Object) {
        outError = std::string("'") + field + "' must be bool/int/float/string";
        return false;
    }
    return true;
}

static bool ParseProperty(const json& propertyJson, PropertyDef& outProperty, std::string& outError)
{
    if (!propertyJson.is_object()) {
        outError = "property entry is not an object";
        return false;
    }
    if (!propertyJson.contains("name") || !propertyJson["name"].is_string()
        || propertyJson["name"].get<std::string>().empty()) {
        outError = "property has missing or empty 'name'";
        return false;
    }
    outProperty.name = propertyJson["name"].get<std::string>();

    outProperty.container = PropertyContainer::None;
    if (propertyJson.contains("container")) {
        if (!propertyJson["container"].is_string()
            || !PropertyContainerFromString(propertyJson["container"].get<std::string>(),
                                            outProperty.container)) {
            outError = "property '" + outProperty.name
                     + "' has invalid 'container' (none/array/set/map)";
            return false;
        }
    }

    std::string typeError;
    if (!ParsePropertyValueType(propertyJson, "type", outProperty.type, typeError)) {
        outError = "property '" + outProperty.name + "': " + typeError;
        return false;
    }

    outProperty.keyType = PinType::String;
    if (outProperty.container == PropertyContainer::Map && propertyJson.contains("keyType")) {
        std::string keyError;
        if (!ParsePropertyValueType(propertyJson, "keyType", outProperty.keyType, keyError)) {
            outError = "property '" + outProperty.name + "': " + keyError;
            return false;
        }
    }

    if (propertyJson.contains("default")) {
        std::string defaultError;
        if (!ParseContainerDefault(propertyJson["default"], outProperty, defaultError)) {
            outError = "property '" + outProperty.name + "': " + defaultError;
            return false;
        }
    } else if (outProperty.container == PropertyContainer::None) {
        outProperty.defaultValue = MakeDefaultValue(outProperty.type);
    }
    return true;
}

static bool ParseNodeClass(const json& classJson, std::string& outError)
{
    if (!classJson.is_object()) {
        outError = "node class entry is not an object";
        return false;
    }
    if (!classJson.contains("name") || !classJson["name"].is_string()) {
        outError = "node class has missing or invalid 'name'";
        return false;
    }
    const std::string name = classJson["name"].get<std::string>();
    if (name.empty()) {
        outError = "node class name is empty";
        return false;
    }
    if (NodeClass::FindByName(name.c_str()) != nullptr) {
        outError = "duplicate node class name: " + name;
        return false;
    }

    if (!classJson.contains("category") || !classJson["category"].is_string()
        || classJson["category"].get<std::string>().empty()) {
        outError = "node class '" + name + "' has missing or empty 'category'";
        return false;
    }
    const std::string category = classJson["category"].get<std::string>();

    std::vector<PinDef> pins;
    if (classJson.contains("pins")) {
        if (!classJson["pins"].is_array()) {
            outError = "node class '" + name + "' has non-array 'pins'";
            return false;
        }
        for (const json& pinJson : classJson["pins"]) {
            PinDef pin;
            std::string pinError;
            if (!ParsePin(pinJson, pin, pinError)) {
                outError = "node class '" + name + "': " + pinError;
                return false;
            }
            pins.push_back(std::move(pin));
        }
    }

    std::vector<PropertyDef> properties;
    if (classJson.contains("properties")) {
        if (!classJson["properties"].is_array()) {
            outError = "node class '" + name + "' has non-array 'properties'";
            return false;
        }
        for (const json& propertyJson : classJson["properties"]) {
            PropertyDef property;
            std::string propertyError;
            if (!ParseProperty(propertyJson, property, propertyError)) {
                outError = "node class '" + name + "': " + propertyError;
                return false;
            }
            properties.push_back(std::move(property));
        }
    }

    std::string execFnName;
    if (classJson.contains("execFn") && classJson["execFn"].is_string()) {
        execFnName = classJson["execFn"].get<std::string>();
    }

    // Constructor self-registers; AdoptDynamic only stores ownership.
    NodeClass::AdoptDynamic(
        std::make_unique<NodeClass>(name, category, std::move(pins), std::move(properties),
                                    std::move(execFnName)));
    return true;
}

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

static json PropertyDefaultToJson(const PropertyDef& property)
{
    switch (property.container) {
    case PropertyContainer::None:
        return ValueToJson(property.defaultValue);
    case PropertyContainer::Array:
    case PropertyContainer::Set: {
        json array = json::array();
        for (const Value& element : property.defaultElements) {
            array.push_back(ValueToJson(element));
        }
        return array;
    }
    case PropertyContainer::Map: {
        json object = json::object();
        for (const std::pair<Value, Value>& entry : property.defaultEntries) {
            object[ValueToString(entry.first)] = ValueToJson(entry.second);
        }
        return object;
    }
    }
    return json();
}

static json BuildClassEntry(const std::string& name, const std::string& category,
                            const std::vector<PinDef>& pins,
                            const std::vector<PropertyDef>& properties,
                            const std::string& execFnName)
{
    json entry;
    entry["name"] = name;
    entry["category"] = category;
    if (!execFnName.empty()) {
        entry["execFn"] = execFnName;
    }
    json pinArray = json::array();
    for (const PinDef& pin : pins) {
        json pinJson;
        pinJson["direction"] = PinDirectionToString(pin.direction);
        pinJson["type"] = PinTypeToString(pin.type);
        pinJson["name"] = pin.name;
        pinArray.push_back(pinJson);
    }
    entry["pins"] = pinArray;
    json propertyArray = json::array();
    for (const PropertyDef& property : properties) {
        json propertyJson;
        propertyJson["name"] = property.name;
        propertyJson["container"] = PropertyContainerToString(property.container);
        propertyJson["type"] = PinTypeToString(property.type);
        if (property.container == PropertyContainer::Map) {
            propertyJson["keyType"] = PinTypeToString(property.keyType);
        }
        propertyJson["default"] = PropertyDefaultToJson(property);
        propertyArray.push_back(propertyJson);
    }
    entry["properties"] = propertyArray;
    return entry;
}

// Reads the class file into outRoot. Missing file yields an empty
// document; invalid JSON fails so the file is never clobbered.
static bool ReadClassFile(const std::string& path, json& outRoot, std::string& outError)
{
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();
        outRoot = json::parse(buffer.str(), nullptr, false);
        if (outRoot.is_discarded()) {
            outError = "existing file has invalid JSON; not overwriting";
            return false;
        }
    }
    if (!outRoot.is_object()) {
        outRoot = json::object();
    }
    if (!outRoot.contains("nodeClasses") || !outRoot["nodeClasses"].is_array()) {
        outRoot["nodeClasses"] = json::array();
    }
    return true;
}

static bool WriteClassFile(const std::string& path, const json& root, std::string& outError)
{
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

bool AppendNodeClassToFile(const std::string& path, const std::string& name,
                           const std::string& category, const std::vector<PinDef>& pins,
                           const std::vector<PropertyDef>& properties,
                           const std::string& execFnName, std::string& outError)
{
    json root;
    if (!ReadClassFile(path, root, outError)) {
        return false;
    }
    root["nodeClasses"].push_back(BuildClassEntry(name, category, pins, properties, execFnName));
    return WriteClassFile(path, root, outError);
}

bool UpdateNodeClassInFile(const std::string& path, const std::string& oldName,
                           const std::string& name, const std::string& category,
                           const std::vector<PinDef>& pins,
                           const std::vector<PropertyDef>& properties,
                           const std::string& execFnName, std::string& outError)
{
    json root;
    if (!ReadClassFile(path, root, outError)) {
        return false;
    }
    for (json& entry : root["nodeClasses"]) {
        if (entry.is_object() && entry.contains("name") && entry["name"].is_string()
            && entry["name"].get<std::string>() == oldName) {
            entry = BuildClassEntry(name, category, pins, properties, execFnName);
            return WriteClassFile(path, root, outError);
        }
    }
    outError = "class not found in file: " + oldName;
    return false;
}

int LoadNodeClassesFromFile(const std::string& path, std::vector<std::string>& outErrors)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        outErrors.push_back("cannot open file: " + path);
        return 0;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    const json root = json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded()) {
        outErrors.push_back("invalid JSON syntax");
        return 0;
    }
    if (!root.is_object() || !root.contains("nodeClasses") || !root["nodeClasses"].is_array()) {
        outErrors.push_back("root must be an object with a 'nodeClasses' array");
        return 0;
    }

    int loadedCount = 0;
    for (const json& classJson : root["nodeClasses"]) {
        std::string error;
        if (ParseNodeClass(classJson, error)) {
            ++loadedCount;
        } else {
            outErrors.push_back(error);
        }
    }
    return loadedCount;
}
