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

    collapsedCategories.clear();
    openFiles.clear();
    activeFilePath.clear();

    if (root.contains("contextMenu") && root["contextMenu"].is_object()) {
        const json& menu = root["contextMenu"];
        if (menu.contains("collapsedCategories") && menu["collapsedCategories"].is_array()) {
            for (const json& entry : menu["collapsedCategories"]) {
                if (entry.is_string() && !entry.get<std::string>().empty()) {
                    collapsedCategories.push_back(entry.get<std::string>());
                }
            }
        }
    }

    if (root.contains("session") && root["session"].is_object()) {
        const json& session = root["session"];
        if (session.contains("openFiles") && session["openFiles"].is_array()) {
            for (const json& entry : session["openFiles"]) {
                OpenFileEntry openFile;
                // Older files stored plain path strings; accept both.
                if (entry.is_string()) {
                    openFile.path = entry.get<std::string>();
                } else if (entry.is_object()
                           && entry.contains("path") && entry["path"].is_string()) {
                    openFile.path = entry["path"].get<std::string>();
                    if (entry.contains("untitled") && entry["untitled"].is_boolean()) {
                        openFile.untitled = entry["untitled"].get<bool>();
                    }
                    if (entry.contains("displayName") && entry["displayName"].is_string()) {
                        openFile.displayName = entry["displayName"].get<std::string>();
                    }
                    if (entry.contains("panX") && entry["panX"].is_number()) {
                        openFile.panX = entry["panX"].get<float>();
                    }
                    if (entry.contains("panY") && entry["panY"].is_number()) {
                        openFile.panY = entry["panY"].get<float>();
                    }
                    if (entry.contains("zoom") && entry["zoom"].is_number()) {
                        openFile.zoom = entry["zoom"].get<float>();
                    }
                }
                if (!openFile.path.empty()) {
                    openFiles.push_back(std::move(openFile));
                }
            }
        }
        if (session.contains("activeFile") && session["activeFile"].is_string()) {
            activeFilePath = session["activeFile"].get<std::string>();
        }
    }
    return true;
}

bool EditorSettings::SaveToFile(const std::string& path) const
{
    json collapsedArray = json::array();
    for (const std::string& category : collapsedCategories) {
        collapsedArray.push_back(category);
    }

    json openArray = json::array();
    for (const OpenFileEntry& openFile : openFiles) {
        json entry;
        entry["path"] = openFile.path;
        entry["untitled"] = openFile.untitled;
        entry["displayName"] = openFile.displayName;
        entry["panX"] = openFile.panX;
        entry["panY"] = openFile.panY;
        entry["zoom"] = openFile.zoom;
        openArray.push_back(entry);
    }

    json root;
    root["contextMenu"]["collapsedCategories"] = collapsedArray;
    root["session"]["openFiles"] = openArray;
    root["session"]["activeFile"] = activeFilePath;

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << root.dump(2, ' ', false, json::error_handler_t::replace) << "\n";
    return file.good();
}
