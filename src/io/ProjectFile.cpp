// Project file save/load and schema migration.

#include "ProjectFile.h"

#include "ProjectExport.h"
#include "V1Import.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>
#include <string>

namespace gau {

using nlohmann::json;

static int ReadSchemaVersion(const std::string& text)
{
    const json root = json::parse(text, nullptr, false);
    if (root.is_discarded() || !root.is_object()) {
        return 0;
    }
    if (root.contains("schemaVersion") && root["schemaVersion"].is_number_integer()) {
        return root["schemaVersion"].get<int>();
    }
    return 0;
}

int LoadProjectText(const std::string& text, Project& project, std::vector<std::string>& errors)
{
    const int version = ReadSchemaVersion(text);
    // Forward migration: versions 0/1 predate functions/variables; those
    // arrays are simply absent and the importers skip them, so no rewrite is
    // needed. Newer-than-current versions still load best-effort.
    ImportV1Definitions(text, project.types, project.classes, errors);
    ImportFunctions(text, project.functions, project.classes, project.types, errors);
    ImportVariables(text, project.variables, project.types, errors);
    ImportComments(text, project.comments, errors);
    ImportV1Graph(text, *project.graph, project.classes, project.types, errors);
    return version;
}

bool SaveProjectFile(const std::string& path, const Project& project)
{
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << ExportProject(project);
    return static_cast<bool>(out);
}

bool LoadProjectFile(const std::string& path, Project& project, std::vector<std::string>& errors)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        errors.push_back("cannot open project file: " + path);
        return false;
    }
    std::ostringstream buffer;
    buffer << in.rdbuf();
    LoadProjectText(buffer.str(), project, errors);
    return true;
}

} // namespace gau
