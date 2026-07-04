#pragma once

// Serializes a v2 Project to the shared JSON format (types + nodeClasses +
// nodes + links) that V1Import also reads, so v2 saves round-trip and stay
// compatible with v1 files.

#include "model/Project.h"

#include <string>

namespace gau {

// Bumped when the on-disk shape changes in a way the loader must migrate.
constexpr int PROJECT_SCHEMA_VERSION = 2;

std::string ExportProject(const Project& project);

} // namespace gau
