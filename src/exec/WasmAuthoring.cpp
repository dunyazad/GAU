// v2 wasm authoring: gau_api.h generation, typed signature scanning and
// entry-source generation over the project TypeRegistry.

#include "WasmAuthoring.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace gau {

namespace {

bool IsIdentifierChar(char c)
{
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

std::string Trim(const std::string& text)
{
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin])) != 0) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
        --end;
    }
    return text.substr(begin, end - begin);
}

const char* ScalarFieldCType(const TypeRegistry& types, TypeId id)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr) {
        return nullptr;
    }
    switch (desc->tag) {
    case TypeTag::Float:
        return "float";
    case TypeTag::Int:
        return "int";
    case TypeTag::Bool:
        return "bool";
    default:
        return nullptr;
    }
}

const StructDef* NestedStruct(const TypeRegistry& types, TypeId id)
{
    const TypeDesc* desc = types.Resolve(id);
    if (desc == nullptr || desc->tag != TypeTag::Struct) {
        return nullptr;
    }
    return types.FindStruct(desc->name);
}

// True when every leaf of the struct (recursively) is float/int/bool, so
// the generated read/write helpers can marshal it.
bool IsMarshallableStruct(const TypeRegistry& types, const StructDef& def)
{
    for (const StructField& field : def.fields) {
        if (ScalarFieldCType(types, field.type) != nullptr) {
            continue;
        }
        const StructDef* nested = NestedStruct(types, field.type);
        if (nested != nullptr && IsMarshallableStruct(types, *nested)) {
            continue;
        }
        return false;
    }
    return !def.fields.empty();
}

// Number of scalar leaves the struct occupies in the flat data index
// space (mirrors the runtime's FlatWasmContext flattening).
int StructLeafSpan(const TypeRegistry& types, const StructDef& def)
{
    int count = 0;
    for (const StructField& field : def.fields) {
        const StructDef* nested = NestedStruct(types, field.type);
        count += (nested != nullptr) ? StructLeafSpan(types, *nested) : 1;
    }
    return count;
}

void EmitStructReadWrite(std::ostream& out, const TypeRegistry& types, const StructDef& def)
{
    out << "\nstruct " << def.name << "\n{\n";
    for (const StructField& field : def.fields) {
        const char* ctype = ScalarFieldCType(types, field.type);
        if (ctype != nullptr) {
            out << "    " << ctype << " " << field.name << ";\n";
        } else {
            const StructDef* nested = NestedStruct(types, field.type);
            out << "    " << (nested != nullptr ? nested->name : "int") << " " << field.name
                << ";\n";
        }
    }
    out << "};\n";

    const int span = StructLeafSpan(types, def);
    out << "\n// Reads " << span << " consecutive data leaves starting at baseIndex.\n";
    out << "inline " << def.name << " gau_read_" << def.name << "(int baseIndex)\n{\n";
    out << "    " << def.name << " value;\n";
    int offset = 0;
    for (const StructField& field : def.fields) {
        const TypeDesc* desc = types.Resolve(field.type);
        const StructDef* nested = NestedStruct(types, field.type);
        if (nested != nullptr) {
            out << "    value." << field.name << " = gau_read_" << nested->name
                << "(baseIndex + " << offset << ");\n";
            offset += StructLeafSpan(types, *nested);
            continue;
        }
        if (desc != nullptr && desc->tag == TypeTag::Float) {
            out << "    value." << field.name << " = static_cast<float>(gau_input_f64(baseIndex + "
                << offset << "));\n";
        } else if (desc != nullptr && desc->tag == TypeTag::Bool) {
            out << "    value." << field.name << " = gau_input_bool(baseIndex + " << offset
                << ") != 0;\n";
        } else {
            out << "    value." << field.name << " = gau_input_i32(baseIndex + " << offset
                << ");\n";
        }
        ++offset;
    }
    out << "    return value;\n}\n";

    out << "\n// Writes " << span << " consecutive data leaves starting at baseIndex.\n";
    out << "inline void gau_write_" << def.name << "(int baseIndex, const " << def.name
        << "& value)\n{\n";
    offset = 0;
    for (const StructField& field : def.fields) {
        const TypeDesc* desc = types.Resolve(field.type);
        const StructDef* nested = NestedStruct(types, field.type);
        if (nested != nullptr) {
            out << "    gau_write_" << nested->name << "(baseIndex + " << offset << ", value."
                << field.name << ");\n";
            offset += StructLeafSpan(types, *nested);
            continue;
        }
        if (desc != nullptr && desc->tag == TypeTag::Float) {
            out << "    gau_output_f64(baseIndex + " << offset << ", static_cast<double>(value."
                << field.name << "));\n";
        } else if (desc != nullptr && desc->tag == TypeTag::Bool) {
            out << "    gau_output_bool(baseIndex + " << offset << ", value." << field.name
                << " ? 1 : 0);\n";
        } else {
            out << "    gau_output_i32(baseIndex + " << offset << ", value." << field.name
                << ");\n";
        }
        ++offset;
    }
    out << "}\n";
}

const char* API_PRELUDE =
    "// Auto-generated by GAU from the project type registry.\n"
    "// Regenerated on every wasm build - do not edit.\n"
    "// Index spaces: gau_input_*/gau_output_* count only data pins\n"
    "// (exec pins excluded), one index per flattened struct leaf;\n"
    "// gau_exec counts only exec output pins.\n"
    "#pragma once\n"
    "\n"
    "extern \"C\" {\n"
    "int    gau_input_i32(int index);\n"
    "double gau_input_f64(int index);\n"
    "int    gau_input_bool(int index);\n"
    "int    gau_input_str(int index, char* buffer, int capacity);\n"
    "int    gau_property_i32(int index);\n"
    "double gau_property_f64(int index);\n"
    "int    gau_property_str(int index, char* buffer, int capacity);\n"
    "void   gau_output_i32(int index, int value);\n"
    "void   gau_output_f64(int index, double value);\n"
    "void   gau_output_bool(int index, int value);\n"
    "void   gau_output_str(int index, const char* text, int length);\n"
    "void   gau_exec(int outputIndex);\n"
    "void   gau_log(const char* text, int length);\n"
    "}\n"
    "\n"
    "// --- Text helpers -------------------------------------------------\n"
    "// Wasm functions build freestanding (-nostdlib): <iostream>,\n"
    "// <string>, snprintf etc. are unavailable. Build text in a GauStr\n"
    "// (alias String) via + concatenation, ftoa and itoa.\n"
    "\n"
    "struct GauStr\n"
    "{\n"
    "    char data[512];\n"
    "    int len;\n"
    "};\n"
    "\n"
    "inline GauStr gau_str(const char* text = \"\")\n"
    "{\n"
    "    GauStr s;\n"
    "    s.len = 0;\n"
    "    while (text[s.len] != 0 && s.len < 511) {\n"
    "        s.data[s.len] = text[s.len];\n"
    "        ++s.len;\n"
    "    }\n"
    "    s.data[s.len] = 0;\n"
    "    return s;\n"
    "}\n"
    "\n"
    "inline GauStr& gau_append(GauStr& s, char c)\n"
    "{\n"
    "    if (s.len < 511) {\n"
    "        s.data[s.len] = c;\n"
    "        ++s.len;\n"
    "        s.data[s.len] = 0;\n"
    "    }\n"
    "    return s;\n"
    "}\n"
    "\n"
    "inline GauStr& gau_append(GauStr& s, const char* text)\n"
    "{\n"
    "    while (*text != 0) {\n"
    "        gau_append(s, *text);\n"
    "        ++text;\n"
    "    }\n"
    "    return s;\n"
    "}\n"
    "\n"
    "inline GauStr& gau_append(GauStr& s, long value)\n"
    "{\n"
    "    unsigned long u;\n"
    "    if (value < 0) {\n"
    "        gau_append(s, '-');\n"
    "        u = static_cast<unsigned long>(-(value + 1)) + 1ul;\n"
    "    } else {\n"
    "        u = static_cast<unsigned long>(value);\n"
    "    }\n"
    "    char digits[20];\n"
    "    int count = 0;\n"
    "    do {\n"
    "        digits[count] = static_cast<char>('0' + u % 10ul);\n"
    "        ++count;\n"
    "        u /= 10ul;\n"
    "    } while (u != 0ul);\n"
    "    while (count > 0) {\n"
    "        --count;\n"
    "        gau_append(s, digits[count]);\n"
    "    }\n"
    "    return s;\n"
    "}\n"
    "\n"
    "inline GauStr& gau_append(GauStr& s, int value)\n"
    "{\n"
    "    return gau_append(s, static_cast<long>(value));\n"
    "}\n"
    "\n"
    "inline GauStr& gau_append(GauStr& s, double value, int decimals = 2)\n"
    "{\n"
    "    if (value < 0.0) {\n"
    "        gau_append(s, '-');\n"
    "        value = -value;\n"
    "    }\n"
    "    double scale = 1.0;\n"
    "    for (int i = 0; i < decimals; ++i) {\n"
    "        scale *= 10.0;\n"
    "    }\n"
    "    value += 0.5 / scale;\n"
    "    unsigned long ip = static_cast<unsigned long>(value);\n"
    "    gau_append(s, static_cast<long>(ip));\n"
    "    if (decimals > 0) {\n"
    "        gau_append(s, '.');\n"
    "        double frac = value - static_cast<double>(ip);\n"
    "        for (int i = 0; i < decimals; ++i) {\n"
    "            frac *= 10.0;\n"
    "            int digit = static_cast<int>(frac);\n"
    "            if (digit > 9) {\n"
    "                digit = 9;\n"
    "            }\n"
    "            frac -= static_cast<double>(digit);\n"
    "            gau_append(s, static_cast<char>('0' + digit));\n"
    "        }\n"
    "    }\n"
    "    return s;\n"
    "}\n"
    "\n"
    "inline void gau_output_str(int index, const GauStr& s)\n"
    "{\n"
    "    gau_output_str(index, s.data, s.len);\n"
    "}\n"
    "\n"
    "inline void gau_log(const GauStr& s)\n"
    "{\n"
    "    gau_log(s.data, s.len);\n"
    "}\n"
    "\n"
    "typedef GauStr String;\n"
    "\n"
    "inline GauStr operator+(GauStr a, const GauStr& b)\n"
    "{\n"
    "    for (int i = 0; i < b.len; ++i) {\n"
    "        gau_append(a, b.data[i]);\n"
    "    }\n"
    "    return a;\n"
    "}\n"
    "\n"
    "inline GauStr operator+(GauStr a, const char* b)\n"
    "{\n"
    "    gau_append(a, b);\n"
    "    return a;\n"
    "}\n"
    "\n"
    "inline GauStr operator+(const char* a, const GauStr& b)\n"
    "{\n"
    "    GauStr s = gau_str(a);\n"
    "    return s + b;\n"
    "}\n"
    "\n"
    "// Decimal text of a float value (default two fraction digits).\n"
    "inline GauStr ftoa(double value, int decimals = 2)\n"
    "{\n"
    "    GauStr s = gau_str();\n"
    "    gau_append(s, value, decimals);\n"
    "    return s;\n"
    "}\n"
    "\n"
    "// Decimal text of an integer value.\n"
    "inline GauStr itoa(long value)\n"
    "{\n"
    "    GauStr s = gau_str();\n"
    "    gau_append(s, value);\n"
    "    return s;\n"
    "}\n";

} // namespace

bool WriteWasmApiHeader(const std::string& path, const TypeRegistry& types,
                        std::string& outError)
{
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        outError = "cannot write " + path;
        return false;
    }
    file << API_PRELUDE;

    // Emit marshallable structs in dependency order: a struct goes out
    // only once all its nested structs are out.
    std::vector<const StructDef*> pending;
    for (const StructDef& def : types.Structs()) {
        if (IsMarshallableStruct(types, def)) {
            pending.push_back(&def);
        }
    }
    std::vector<std::string> emitted;
    const auto isEmitted = [&emitted](const std::string& name) {
        for (const std::string& e : emitted) {
            if (e == name) {
                return true;
            }
        }
        return false;
    };
    bool progressed = true;
    while (!pending.empty() && progressed) {
        progressed = false;
        for (std::size_t i = 0; i < pending.size(); ++i) {
            const StructDef* def = pending[i];
            bool ready = true;
            for (const StructField& field : def->fields) {
                const StructDef* nested = NestedStruct(types, field.type);
                if (nested != nullptr && !isEmitted(nested->name)) {
                    ready = false;
                    break;
                }
            }
            if (!ready) {
                continue;
            }
            EmitStructReadWrite(file, types, *def);
            emitted.push_back(def->name);
            pending.erase(pending.begin() + static_cast<std::ptrdiff_t>(i));
            progressed = true;
            break;
        }
    }
    return file.good();
}

namespace {

// Blanks // and /* */ comments (and string/char literal contents) so the
// scanner never matches text inside them. Length is preserved.
std::string StripCommentsAndLiterals(const std::string& source)
{
    std::string result = source;
    enum class State
    {
        Code,
        LineComment,
        BlockComment,
        StringLit,
        CharLit,
    };
    State state = State::Code;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const char c = result[i];
        const char next = (i + 1 < result.size()) ? result[i + 1] : '\0';
        switch (state) {
        case State::Code:
            if (c == '/' && next == '/') {
                state = State::LineComment;
                result[i] = ' ';
            } else if (c == '/' && next == '*') {
                state = State::BlockComment;
                result[i] = ' ';
            } else if (c == '"') {
                state = State::StringLit;
            } else if (c == '\'') {
                state = State::CharLit;
            }
            break;
        case State::LineComment:
            if (c == '\n') {
                state = State::Code;
            } else {
                result[i] = ' ';
            }
            break;
        case State::BlockComment:
            if (c == '*' && next == '/') {
                result[i] = ' ';
                result[i + 1] = ' ';
                ++i;
                state = State::Code;
            } else if (c != '\n') {
                result[i] = ' ';
            }
            break;
        case State::StringLit:
            if (c == '\\') {
                result[i] = ' ';
                if (i + 1 < result.size()) {
                    result[i + 1] = ' ';
                    ++i;
                }
            } else if (c == '"') {
                state = State::Code;
            } else {
                result[i] = ' ';
            }
            break;
        case State::CharLit:
            if (c == '\\') {
                result[i] = ' ';
                if (i + 1 < result.size()) {
                    result[i + 1] = ' ';
                    ++i;
                }
            } else if (c == '\'') {
                state = State::Code;
            } else {
                result[i] = ' ';
            }
            break;
        }
    }
    return result;
}

std::vector<std::string> TokenizeTypeText(const std::string& text, bool& outHasPointer)
{
    std::vector<std::string> tokens;
    outHasPointer = false;
    std::string current;
    for (char c : text) {
        if (IsIdentifierChar(c)) {
            current += c;
            continue;
        }
        if (!current.empty()) {
            tokens.push_back(current);
            current.clear();
        }
        if (c == '*') {
            outHasPointer = true;
        }
    }
    if (!current.empty()) {
        tokens.push_back(current);
    }
    return tokens;
}

// Maps a single type token to a value kind. Returns false for unknown
// types (outError explains).
bool ResolveValueType(const std::string& token, const TypeRegistry& types,
                      WasmSigParam& outParam, std::string& outError)
{
    if (token == "int" || token == "long") {
        outParam.kind = WasmSigKind::Int;
        outParam.typeText = token;
        return true;
    }
    if (token == "float" || token == "double") {
        outParam.kind = WasmSigKind::Float;
        outParam.typeText = token;
        return true;
    }
    if (token == "bool") {
        outParam.kind = WasmSigKind::Bool;
        outParam.typeText = "bool";
        return true;
    }
    if (token == "String" || token == "GauStr") {
        outParam.kind = WasmSigKind::Str;
        outParam.typeText = "GauStr";
        return true;
    }
    const StructDef* def = types.FindStruct(token);
    if (def != nullptr && IsMarshallableStruct(types, *def)) {
        outParam.kind = WasmSigKind::Struct;
        outParam.typeText = token;
        outParam.structName = token;
        return true;
    }
    outError = "unknown type '" + token + "' (usable: int, long, float, double, bool,"
               " String, or a struct from the type registry)";
    return false;
}

// Parses one parameter declaration ("const Vector3f& v"). Unnamed
// parameters get argN.
bool ParseParam(const std::string& text, int index, const TypeRegistry& types,
                WasmSigParam& outParam, std::string& outError)
{
    bool hasPointer = false;
    std::vector<std::string> tokens = TokenizeTypeText(text, hasPointer);
    if (hasPointer) {
        outError = "pointer parameters are not supported: '" + Trim(text) + "'";
        return false;
    }
    std::vector<std::string> meaningful;
    for (const std::string& token : tokens) {
        if (token != "const" && token != "struct") {
            meaningful.push_back(token);
        }
    }
    if (meaningful.empty() || meaningful.size() > 2) {
        outError = "cannot parse parameter: '" + Trim(text) + "'";
        return false;
    }

    std::string name;
    if (meaningful.size() == 2) {
        name = meaningful.back();
    }
    if (name.empty()) {
        name = "arg" + std::to_string(index);
    }
    if (!ResolveValueType(meaningful[0], types, outParam, outError)) {
        return false;
    }
    outParam.name = name;
    return true;
}

struct Candidate
{
    WasmSignature signature;
    bool typed = false;
};

// Parses the extern "C" occurrence starting after the closing quote of
// "C". Returns false when the text there is not a function definition we
// understand; typed/unsupported results land in outCandidate/outError.
bool ParseCandidate(const std::string& text, std::size_t position, const TypeRegistry& types,
                    Candidate& outCandidate, bool& outUnsupported, std::string& outError)
{
    const std::size_t openParen = text.find('(', position);
    if (openParen == std::string::npos) {
        return false;
    }
    for (std::size_t i = position; i < openParen; ++i) {
        if (text[i] == '{' || text[i] == '}' || text[i] == ';') {
            return false;
        }
    }
    const std::size_t closeParen = text.find(')', openParen);
    if (closeParen == std::string::npos) {
        return false;
    }
    std::size_t after = closeParen + 1;
    while (after < text.size() && std::isspace(static_cast<unsigned char>(text[after])) != 0) {
        ++after;
    }
    if (after >= text.size() || text[after] != '{') {
        return false;
    }

    const std::string head = text.substr(position, openParen - position);
    bool headPointer = false;
    std::vector<std::string> headTokens = TokenizeTypeText(head, headPointer);
    if (headTokens.size() < 2) {
        return false;
    }
    const std::string functionName = headTokens.back();
    headTokens.pop_back();
    std::vector<std::string> returnTokens;
    for (const std::string& token : headTokens) {
        if (token != "const" && token != "struct" && token != "inline" && token != "static") {
            returnTokens.push_back(token);
        }
    }
    if (headPointer || returnTokens.size() != 1) {
        outUnsupported = true;
        outError = "cannot parse return type of '" + functionName + "'";
        return false;
    }

    WasmSignature signature;
    signature.functionName = functionName;
    signature.returnTypeText = returnTokens[0];
    signature.paramListText = Trim(text.substr(openParen + 1, closeParen - openParen - 1));

    bool typed = false;
    if (!signature.paramListText.empty() && signature.paramListText != "void") {
        std::stringstream stream(signature.paramListText);
        std::string piece;
        int index = 0;
        while (std::getline(stream, piece, ',')) {
            WasmSigParam param;
            if (!ParseParam(piece, index, types, param, outError)) {
                outUnsupported = true;
                outError = "'" + functionName + "': " + outError;
                return false;
            }
            signature.params.push_back(std::move(param));
            ++index;
        }
        typed = true;
    }

    if (signature.returnTypeText == "void") {
        signature.returnsVoid = true;
    } else {
        if (!ResolveValueType(signature.returnTypeText, types, signature.returnValue, outError)) {
            outUnsupported = true;
            outError = "'" + functionName + "': " + outError;
            return false;
        }
        signature.returnsVoid = false;
        signature.returnValue.name = "result";
        typed = true;
    }

    outCandidate.signature = std::move(signature);
    outCandidate.typed = typed;
    return true;
}

int ParamLeafSpan(const TypeRegistry& types, const WasmSigParam& param)
{
    if (param.kind != WasmSigKind::Struct) {
        return 1;
    }
    const StructDef* def = types.FindStruct(param.structName);
    return (def != nullptr) ? StructLeafSpan(types, *def) : 1;
}

} // namespace

WasmSigScan ScanWasmSignature(const std::string& source, const std::string& preferredName,
                              const TypeRegistry& types, WasmSignature& outSignature,
                              std::string& outError)
{
    const std::string text = StripCommentsAndLiterals(source);

    std::vector<Candidate> typedCandidates;
    std::size_t search = 0;
    while (true) {
        const std::size_t externPos = text.find("extern", search);
        if (externPos == std::string::npos) {
            break;
        }
        search = externPos + 6;
        if (externPos > 0 && IsIdentifierChar(text[externPos - 1])) {
            continue;
        }
        if (externPos + 6 < text.size() && IsIdentifierChar(text[externPos + 6])) {
            continue;
        }
        std::size_t quote = externPos + 6;
        while (quote < text.size() && std::isspace(static_cast<unsigned char>(text[quote])) != 0) {
            ++quote;
        }
        if (quote >= text.size() || text[quote] != '"') {
            continue;
        }
        const std::size_t closeQuote = text.find('"', quote + 1);
        if (closeQuote == std::string::npos) {
            break;
        }

        Candidate candidate;
        bool unsupported = false;
        if (ParseCandidate(text, closeQuote + 1, types, candidate, unsupported, outError)) {
            if (candidate.typed) {
                typedCandidates.push_back(std::move(candidate));
            }
        } else if (unsupported) {
            return WasmSigScan::Unsupported;
        }
    }

    if (typedCandidates.empty()) {
        return WasmSigScan::NoTypedFunction;
    }
    for (Candidate& candidate : typedCandidates) {
        if (candidate.signature.functionName == preferredName) {
            outSignature = std::move(candidate.signature);
            return WasmSigScan::Found;
        }
    }
    outSignature = std::move(typedCandidates[0].signature);
    return WasmSigScan::Found;
}

std::string WasmEntryExportName(const std::string& functionName)
{
    return functionName + "__entry";
}

bool BuildPinsFromWasmSignature(const WasmSignature& signature, TypeRegistry& types,
                                std::vector<PinDef>& outPins, std::string& outError)
{
    outPins.clear();

    const auto scalarType = [&types](WasmSigKind kind) {
        switch (kind) {
        case WasmSigKind::Int:
            return types.Builtin(TypeTag::Int);
        case WasmSigKind::Float:
            return types.Builtin(TypeTag::Float);
        case WasmSigKind::Bool:
            return types.Builtin(TypeTag::Bool);
        case WasmSigKind::Str:
            return types.Builtin(TypeTag::String);
        case WasmSigKind::Struct:
            break;
        }
        return types.Builtin(TypeTag::Int);
    };

    if (signature.returnsVoid) {
        PinDef execIn;
        execIn.direction = PinDirection::Input;
        execIn.type = types.Builtin(TypeTag::Exec);
        outPins.push_back(std::move(execIn));
    }
    for (const WasmSigParam& param : signature.params) {
        PinDef pin;
        pin.direction = PinDirection::Input;
        pin.name = param.name;
        if (param.kind == WasmSigKind::Struct) {
            if (types.FindStruct(param.structName) == nullptr) {
                outError = "'" + param.structName + "' is not a registered struct";
                return false;
            }
            pin.type = types.UserType(param.structName);
        } else {
            pin.type = scalarType(param.kind);
        }
        outPins.push_back(std::move(pin));
    }
    if (signature.returnsVoid) {
        PinDef execOut;
        execOut.direction = PinDirection::Output;
        execOut.type = types.Builtin(TypeTag::Exec);
        execOut.name = "then";
        outPins.push_back(std::move(execOut));
    } else {
        PinDef result;
        result.direction = PinDirection::Output;
        result.name = "result";
        if (signature.returnValue.kind == WasmSigKind::Struct) {
            if (types.FindStruct(signature.returnValue.structName) == nullptr) {
                outError = "'" + signature.returnValue.structName
                         + "' is not a registered struct";
                return false;
            }
            result.type = types.UserType(signature.returnValue.structName);
        } else {
            result.type = scalarType(signature.returnValue.kind);
        }
        outPins.push_back(std::move(result));
    }
    return true;
}

std::string GenerateWasmEntrySource(const WasmSignature& signature, const TypeRegistry& types)
{
    std::stringstream out;
    out << "// Auto-generated by GAU for '" << signature.functionName
        << "' -- do not edit.\n"
           "// Bridges the flattened data-leaf host ABI to the typed signature;\n"
           "// the node class binds to wasm:"
        << WasmEntryExportName(signature.functionName) << ".\n"
        << "#include \"gau_api.h\"\n\n";

    out << "extern \"C\" " << signature.returnTypeText << " " << signature.functionName << "("
        << signature.paramListText << ");\n\n";

    out << "extern \"C\" void " << WasmEntryExportName(signature.functionName) << "(void)\n{\n";

    int leafIndex = 0;
    for (std::size_t i = 0; i < signature.params.size(); ++i) {
        const WasmSigParam& param = signature.params[i];
        const std::string local = "p" + std::to_string(i);
        switch (param.kind) {
        case WasmSigKind::Int:
            out << "    " << param.typeText << " " << local << " = gau_input_i32(" << leafIndex
                << ");\n";
            break;
        case WasmSigKind::Float:
            out << "    " << param.typeText << " " << local << " = static_cast<"
                << param.typeText << ">(gau_input_f64(" << leafIndex << "));\n";
            break;
        case WasmSigKind::Bool:
            out << "    bool " << local << " = gau_input_bool(" << leafIndex << ") != 0;\n";
            break;
        case WasmSigKind::Str:
            out << "    GauStr " << local << " = gau_str();\n"
                << "    " << local << ".len = gau_input_str(" << leafIndex << ", " << local
                << ".data, 511);\n"
                << "    if (" << local << ".len < 0) { " << local << ".len = 0; }\n"
                << "    " << local << ".data[" << local << ".len] = 0;\n";
            break;
        case WasmSigKind::Struct:
            out << "    " << param.structName << " " << local << " = gau_read_"
                << param.structName << "(" << leafIndex << ");\n";
            break;
        }
        leafIndex += ParamLeafSpan(types, param);
    }

    std::string call = signature.functionName + "(";
    for (std::size_t i = 0; i < signature.params.size(); ++i) {
        if (i > 0) {
            call += ", ";
        }
        call += "p" + std::to_string(i);
    }
    call += ")";

    if (signature.returnsVoid) {
        out << "    " << call << ";\n"
            << "    gau_exec(0);\n";
    } else {
        switch (signature.returnValue.kind) {
        case WasmSigKind::Int:
            out << "    gau_output_i32(0, static_cast<int>(" << call << "));\n";
            break;
        case WasmSigKind::Float:
            out << "    gau_output_f64(0, static_cast<double>(" << call << "));\n";
            break;
        case WasmSigKind::Bool:
            out << "    gau_output_bool(0, " << call << " ? 1 : 0);\n";
            break;
        case WasmSigKind::Str:
            out << "    GauStr result = " << call << ";\n"
                << "    gau_output_str(0, result.data, result.len);\n";
            break;
        case WasmSigKind::Struct:
            out << "    " << signature.returnValue.structName << " result = " << call << ";\n"
                << "    gau_write_" << signature.returnValue.structName << "(0, result);\n";
            break;
        }
    }

    out << "}\n";
    return out.str();
}

} // namespace gau
