#include "EditorSettings.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

using nlohmann::json;

bool EditorSettings::LoadFromFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file.is_open()) {
        return false;
    }
    std::stringstream buffer;
    buffer << file.rdbuf();

    const json root = json::parse(buffer.str(), nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return false;
    }

    for (bool& flag : categoryCollapsed) {
        flag = false;
    }

    if (root.contains("contextMenu") && root["contextMenu"].is_object()) {
        const json& menu = root["contextMenu"];
        if (menu.contains("collapsedCategories") && menu["collapsedCategories"].is_array()) {
            for (const json& entry : menu["collapsedCategories"]) {
                if (!entry.is_string()) {
                    continue;
                }
                NodeCategory category = NodeCategory::Function;
                if (NodeCategoryFromString(entry.get<std::string>(), category)) {
                    categoryCollapsed[NodeCategoryIndex(category)] = true;
                }
            }
        }
    }
    return true;
}

bool EditorSettings::SaveToFile(const std::string& path) const
{
    json collapsedArray = json::array();
    for (NodeCategory category : ALL_NODE_CATEGORIES) {
        if (categoryCollapsed[NodeCategoryIndex(category)]) {
            collapsedArray.push_back(NodeCategoryToString(category));
        }
    }

    json root;
    root["contextMenu"]["collapsedCategories"] = collapsedArray;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    return file.good();
}
