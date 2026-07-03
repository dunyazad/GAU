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

    NodeCategory category = NodeCategory::Function;
    if (!classJson.contains("category") || !classJson["category"].is_string()
        || !NodeCategoryFromString(classJson["category"].get<std::string>(), category)) {
        outError = "node class '" + name + "' has missing or invalid 'category'";
        return false;
    }

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

    // Constructor self-registers; AdoptDynamic only stores ownership.
    NodeClass::AdoptDynamic(std::make_unique<NodeClass>(name, category, std::move(pins)));
    return true;
}

bool AppendNodeClassToFile(const std::string& path, const std::string& name,
                           NodeCategory category, const std::vector<PinDef>& pins,
                           std::string& outError)
{
    json root;
    {
        std::ifstream file(path);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            root = json::parse(buffer.str(), nullptr, false);
            if (root.is_discarded()) {
                outError = "existing file has invalid JSON; not overwriting";
                return false;
            }
        }
    }
    if (!root.is_object()) {
        root = json::object();
    }
    if (!root.contains("nodeClasses") || !root["nodeClasses"].is_array()) {
        root["nodeClasses"] = json::array();
    }

    json entry;
    entry["name"] = name;
    entry["category"] = NodeCategoryToString(category);
    json pinArray = json::array();
    for (const PinDef& pin : pins) {
        json pinJson;
        pinJson["direction"] = PinDirectionToString(pin.direction);
        pinJson["type"] = PinTypeToString(pin.type);
        pinJson["name"] = pin.name;
        pinArray.push_back(pinJson);
    }
    entry["pins"] = pinArray;
    root["nodeClasses"].push_back(entry);

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
