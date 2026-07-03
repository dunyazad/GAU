#pragma once

#include <string>

// Generates a random UUID v4 string ("xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx").
// Used as the persistent identity of graph elements; runtime lookups
// keep using the fast integer ids.
std::string GenerateGuid();
