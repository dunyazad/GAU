#pragma once

// Project file save/load (SRS FR-PRJ-1, FR-PRJ-3). Wraps ExportProject and
// the V1Import loaders into a single round-trip over the shared JSON, reads
// the schema version, and applies forward migration. Runtime behaviors are
// NOT bound here (io must not depend on exec); after loading, the caller
// rebinds via RegisterStructNodes/RegisterFunctionNodes/RegisterVariableNodes.

#include "model/Project.h"

#include <string>
#include <vector>

namespace gau {

// Loads model data (types, classes, functions, variables, graph) from JSON
// text into an empty project, in dependency order. Returns the schema
// version found in the document (0 when absent, i.e. a pre-versioned file).
int LoadProjectText(const std::string& text, Project& project, std::vector<std::string>& errors);

// File wrappers. Return false on an I/O failure (path unwritable/unreadable).
bool SaveProjectFile(const std::string& path, const Project& project);
bool LoadProjectFile(const std::string& path, Project& project, std::vector<std::string>& errors);

} // namespace gau
