#pragma once

#include <string>

// Generates wasm_src/gau_api.h from the current NodeClass registry:
// the host import declarations plus, for every data-carrier class
// (a valid identifier name and only scalar float/int/bool properties),
// a struct and gau_read_<Name>/gau_write_<Name> pin helpers. Runs at
// startup and before every wasm build so runtime-added classes are
// immediately usable as types in function sources.
bool WriteWasmApiHeader(const std::string& path, std::string& outError);
