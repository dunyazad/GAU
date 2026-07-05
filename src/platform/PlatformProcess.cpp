#include "PlatformProcess.h"

#include <cstdio>

#if defined(_WIN32)
#define GAU_POPEN _popen
#define GAU_PCLOSE _pclose
#else
#define GAU_POPEN popen
#define GAU_PCLOSE pclose
#endif

bool RunCommandCaptured(const std::string& commandLine, std::string& outOutput,
                        int& outExitCode)
{
    outOutput.clear();
    outExitCode = -1;

    // Append the stderr redirect and nothing else. cmd.exe /C strips the outer
    // quote pair only when the whole string both starts and ends with a quote;
    // the trailing " 2>&1" makes the last character a non-quote, so an exe path
    // quoted at the front (e.g. "C:/.../clang.exe" ... "src.cpp") is parsed as
    // written instead of having its inner quotes stripped/merged. Wrapping the
    // line in an extra quote pair (the earlier approach) left the string ending
    // in "1", which cmd then failed to parse, so the command never ran and its
    // real diagnostics were lost.
    const std::string fullCommand = commandLine + " 2>&1";

    FILE* pipe = GAU_POPEN(fullCommand.c_str(), "r");
    if (pipe == nullptr) {
        return false;
    }

    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        outOutput += buffer;
    }
    outExitCode = GAU_PCLOSE(pipe);
    return true;
}
