// Typed wasm signature scanning and entry-source generation.

#include "WasmSignature.h"

#include "WasmApiHeader.h"

#include <cctype>
#include <sstream>

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

bool IsScalarTypeToken(const std::string& token)
{
    return token == "int" || token == "long" || token == "float" || token == "double"
        || token == "bool" || token == "String" || token == "GauStr";
}

const NodeClass* FindDataClass(const std::string& name)
{
    const NodeClass* nodeClass = NodeClass::FindByName(name.c_str());
    if (nodeClass != nullptr && IsWasmDataCarrierClass(*nodeClass)) {
        return nodeClass;
    }
    return nullptr;
}

// Maps a single type token to a value kind. Returns false for unknown
// types (outError explains).
bool ResolveValueType(const std::string& token, WasmSigParam& outParam, std::string& outError)
{
    if (token == "int" || token == "long") {
        outParam.kind = WasmValueKind::Int;
        outParam.typeText = token;
        return true;
    }
    if (token == "float" || token == "double") {
        outParam.kind = WasmValueKind::Float;
        outParam.typeText = token;
        return true;
    }
    if (token == "bool") {
        outParam.kind = WasmValueKind::Bool;
        outParam.typeText = "bool";
        return true;
    }
    if (token == "String" || token == "GauStr") {
        outParam.kind = WasmValueKind::Str;
        outParam.typeText = "GauStr";
        return true;
    }
    if (FindDataClass(token) != nullptr) {
        outParam.kind = WasmValueKind::Struct;
        outParam.typeText = token;
        outParam.structName = token;
        return true;
    }
    outError = "unknown type '" + token + "' (usable: int, long, float, double, bool,"
               " String, or a data class from gau_api.h)";
    return false;
}

// Parses one parameter declaration ("const Vector3f& v"). Unnamed
// parameters get argN.
bool ParseParam(const std::string& text, int index, WasmSigParam& outParam,
                std::string& outError)
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
    if (meaningful.empty()) {
        outError = "cannot parse parameter: '" + Trim(text) + "'";
        return false;
    }

    std::string typeToken = meaningful[0];
    std::string name;
    if (meaningful.size() >= 2) {
        name = meaningful.back();
        if (meaningful.size() > 2) {
            outError = "cannot parse parameter: '" + Trim(text) + "'";
            return false;
        }
    }
    if (name.empty()) {
        name = "arg" + std::to_string(index);
    }
    if (!ResolveValueType(typeToken, outParam, outError)) {
        return false;
    }
    outParam.name = name;
    return true;
}

// One scanned definition candidate.
struct Candidate
{
    WasmSignature signature;
    bool typed = false;
};

// Parses the extern "C" occurrence starting after the closing quote of
// "C". Returns false when the text there is not a function definition we
// understand (declaration, block form, ...); typed/unsupported results
// land in outCandidate/outError.
bool ParseCandidate(const std::string& text, std::size_t position, Candidate& outCandidate,
                    bool& outUnsupported, std::string& outError)
{
    const std::size_t openParen = text.find('(', position);
    if (openParen == std::string::npos) {
        return false;
    }
    // Reject the block form (extern "C" { ... }) and anything that is not
    // a single head of "return-type name(": a brace or semicolon before
    // the parenthesis means the paren belongs to something else.
    for (std::size_t i = position; i < openParen; ++i) {
        if (text[i] == '{' || text[i] == '}' || text[i] == ';') {
            return false;
        }
    }
    const std::size_t closeParen = text.find(')', openParen);
    if (closeParen == std::string::npos) {
        return false;
    }
    // A definition follows with '{'; declarations (';') are skipped.
    std::size_t after = closeParen + 1;
    while (after < text.size() && std::isspace(static_cast<unsigned char>(text[after])) != 0) {
        ++after;
    }
    if (after >= text.size() || text[after] != '{') {
        return false;
    }

    // Head = return type + function name.
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

    // Parameters.
    bool typed = false;
    if (!signature.paramListText.empty() && signature.paramListText != "void") {
        std::stringstream stream(signature.paramListText);
        std::string piece;
        int index = 0;
        while (std::getline(stream, piece, ',')) {
            WasmSigParam param;
            if (!ParseParam(piece, index, param, outError)) {
                outUnsupported = true;
                outError = "'" + functionName + "': " + outError;
                return false;
            }
            signature.params.push_back(std::move(param));
            ++index;
        }
        typed = true;
    }

    // Return value.
    if (signature.returnTypeText == "void") {
        signature.returnsVoid = true;
    } else {
        if (!ResolveValueType(signature.returnTypeText, signature.returnValue, outError)) {
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

int StructFieldCount(const std::string& structName)
{
    const NodeClass* nodeClass = FindDataClass(structName);
    return nodeClass != nullptr ? static_cast<int>(nodeClass->GetProperties().size()) : 0;
}

} // namespace

WasmSignatureScan ScanWasmSignature(const std::string& source, const std::string& preferredName,
                                    WasmSignature& outSignature, std::string& outError)
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
        // Word-boundary check for the extern keyword.
        if (externPos > 0 && IsIdentifierChar(text[externPos - 1])) {
            continue;
        }
        if (externPos + 6 < text.size() && IsIdentifierChar(text[externPos + 6])) {
            continue;
        }
        // Expect "C" (the literal contents are blanked, so match quotes).
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
        if (ParseCandidate(text, closeQuote + 1, candidate, unsupported, outError)) {
            if (candidate.typed) {
                typedCandidates.push_back(std::move(candidate));
            }
        } else if (unsupported) {
            return WasmSignatureScan::Unsupported;
        }
    }

    if (typedCandidates.empty()) {
        return WasmSignatureScan::NoTypedFunction;
    }
    for (Candidate& candidate : typedCandidates) {
        if (candidate.signature.functionName == preferredName) {
            outSignature = std::move(candidate.signature);
            return WasmSignatureScan::Found;
        }
    }
    outSignature = std::move(typedCandidates[0].signature);
    return WasmSignatureScan::Found;
}

std::string WasmEntryExportName(const std::string& functionName)
{
    return functionName + "__entry";
}

bool BuildPinsFromWasmSignature(const WasmSignature& signature, std::vector<PinDef>& outPins,
                                std::string& outError)
{
    outPins.clear();
    const auto appendScalar = [&outPins](PinDirection direction, WasmValueKind kind,
                                         const std::string& name) {
        PinDef pin;
        pin.direction = direction;
        pin.name = name;
        switch (kind) {
        case WasmValueKind::Int:
            pin.type = PinType::Int;
            break;
        case WasmValueKind::Float:
            pin.type = PinType::Float;
            break;
        case WasmValueKind::Bool:
            pin.type = PinType::Bool;
            break;
        case WasmValueKind::Str:
            pin.type = PinType::String;
            break;
        case WasmValueKind::Struct:
            break;
        }
        outPins.push_back(std::move(pin));
    };
    const auto appendStruct = [&outPins, &outError](PinDirection direction,
                                                    const std::string& structName,
                                                    const std::string& baseName) -> bool {
        const NodeClass* nodeClass = FindDataClass(structName);
        if (nodeClass == nullptr) {
            outError = "'" + structName + "' is not a data class";
            return false;
        }
        for (const PropertyDef& def : nodeClass->GetProperties()) {
            PinDef pin;
            pin.direction = direction;
            pin.type = def.type;
            pin.name = baseName.empty() ? def.name : baseName + "_" + def.name;
            outPins.push_back(std::move(pin));
        }
        return true;
    };

    // A void return means a side-effect node: it runs on the exec flow
    // (exec in, data inputs, exec out) instead of being pulled as a Pure
    // value node -- a Pure node without outputs would never evaluate.
    if (signature.returnsVoid) {
        PinDef execIn;
        execIn.direction = PinDirection::Input;
        execIn.type = PinType::Exec;
        outPins.push_back(execIn);
    }
    for (const WasmSigParam& param : signature.params) {
        if (param.kind == WasmValueKind::Struct) {
            if (!appendStruct(PinDirection::Input, param.structName, param.name)) {
                return false;
            }
        } else {
            appendScalar(PinDirection::Input, param.kind, param.name);
        }
    }
    if (signature.returnsVoid) {
        PinDef execOut;
        execOut.direction = PinDirection::Output;
        execOut.type = PinType::Exec;
        execOut.name = "then";
        outPins.push_back(execOut);
    } else if (signature.returnValue.kind == WasmValueKind::Struct) {
        if (!appendStruct(PinDirection::Output, signature.returnValue.structName, "")) {
            return false;
        }
    } else {
        appendScalar(PinDirection::Output, signature.returnValue.kind, "result");
    }
    return true;
}

std::string GenerateWasmEntrySource(const WasmSignature& signature)
{
    std::stringstream out;
    out << "// Auto-generated by GAU for '" << signature.functionName
        << "' -- do not edit.\n"
           "// Bridges the index-based host ABI to the typed signature; the\n"
           "// node class binds to wasm:"
        << WasmEntryExportName(signature.functionName) << ".\n"
        << "#include \"gau_api.h\"\n\n";

    out << "extern \"C\" " << signature.returnTypeText << " " << signature.functionName << "("
        << signature.paramListText << ");\n\n";

    out << "extern \"C\" void " << WasmEntryExportName(signature.functionName) << "(void)\n{\n";

    // Data indices never include exec pins (separate ABI index spaces), so
    // inputs start at 0 for Pure and exec nodes alike.
    int pinIndex = 0;
    for (std::size_t i = 0; i < signature.params.size(); ++i) {
        const WasmSigParam& param = signature.params[i];
        const std::string local = "p" + std::to_string(i);
        switch (param.kind) {
        case WasmValueKind::Int:
            out << "    " << param.typeText << " " << local << " = gau_input_i32(" << pinIndex
                << ");\n";
            ++pinIndex;
            break;
        case WasmValueKind::Float:
            out << "    " << param.typeText << " " << local << " = static_cast<"
                << param.typeText << ">(gau_input_f64(" << pinIndex << "));\n";
            ++pinIndex;
            break;
        case WasmValueKind::Bool:
            out << "    bool " << local << " = gau_input_bool(" << pinIndex << ") != 0;\n";
            ++pinIndex;
            break;
        case WasmValueKind::Str:
            out << "    GauStr " << local << " = gau_str();\n"
                << "    " << local << ".len = gau_input_str(" << pinIndex << ", " << local
                << ".data, 511);\n"
                << "    if (" << local << ".len < 0) { " << local << ".len = 0; }\n"
                << "    " << local << ".data[" << local << ".len] = 0;\n";
            ++pinIndex;
            break;
        case WasmValueKind::Struct:
            out << "    " << param.structName << " " << local << " = gau_read_"
                << param.structName << "(" << pinIndex << ");\n";
            pinIndex += StructFieldCount(param.structName);
            break;
        }
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
        case WasmValueKind::Int:
            out << "    gau_output_i32(0, static_cast<int>(" << call << "));\n";
            break;
        case WasmValueKind::Float:
            out << "    gau_output_f64(0, static_cast<double>(" << call << "));\n";
            break;
        case WasmValueKind::Bool:
            out << "    gau_output_bool(0, " << call << " ? 1 : 0);\n";
            break;
        case WasmValueKind::Str:
            out << "    GauStr result = " << call << ";\n"
                << "    gau_output_str(0, result.data, result.len);\n";
            break;
        case WasmValueKind::Struct:
            out << "    " << signature.returnValue.structName << " result = " << call << ";\n"
                << "    gau_write_" << signature.returnValue.structName << "(0, result);\n";
            break;
        }
    }

    out << "}\n";
    return out.str();
}
