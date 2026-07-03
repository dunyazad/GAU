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

#if defined(_WIN32)
    // cmd.exe strips the outermost quotes of the /C string; wrapping the
    // whole line keeps embedded quoted paths intact.
    const std::string fullCommand = "\"" + commandLine + "\" 2>&1";
#else
    const std::string fullCommand = commandLine + " 2>&1";
#endif

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
