#include "PlatformProcess.h"

#include <cstdio>

#if defined(_WIN32)

#include <windows.h>

// Runs the command line directly through CreateProcess with stdout/stderr
// captured over an anonymous pipe. No shell is involved, so cmd.exe's
// quote-stripping rules (which used to truncate a quoted exe path like
// "C:/Program Files/LLVM/bin/clang.exe" down to C:/Program) never apply;
// the quoted command line is parsed by the child's own argv parsing.
bool RunCommandCaptured(const std::string& commandLine, std::string& outOutput,
                        int& outExitCode)
{
    outOutput.clear();
    outExitCode = -1;

    SECURITY_ATTRIBUTES security{};
    security.nLength = sizeof(security);
    security.bInheritHandle = TRUE;

    HANDLE readEnd = nullptr;
    HANDLE writeEnd = nullptr;
    if (!CreatePipe(&readEnd, &writeEnd, &security, 0)) {
        return false;
    }
    // The parent's read end must not leak into the child, or the pipe never
    // signals EOF after the child exits.
    SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = writeEnd;
    startup.hStdError = writeEnd;
    startup.hStdInput = nullptr;

    // CreateProcess may modify the command line buffer in place.
    std::string mutableCommand = commandLine;
    PROCESS_INFORMATION process{};
    const BOOL created =
        CreateProcessA(nullptr, mutableCommand.data(), nullptr, nullptr, TRUE,
                       CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process);
    CloseHandle(writeEnd);
    if (!created) {
        CloseHandle(readEnd);
        return false;
    }

    char buffer[512];
    DWORD bytesRead = 0;
    while (ReadFile(readEnd, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
        outOutput.append(buffer, bytesRead);
    }
    CloseHandle(readEnd);

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 0;
    if (GetExitCodeProcess(process.hProcess, &exitCode)) {
        outExitCode = static_cast<int>(exitCode);
    }
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    return true;
}

#else

// POSIX: popen already runs through /bin/sh, whose quoting rules handle a
// quoted exe path correctly; only stderr needs redirecting into the pipe.
bool RunCommandCaptured(const std::string& commandLine, std::string& outOutput,
                        int& outExitCode)
{
    outOutput.clear();
    outExitCode = -1;

    const std::string fullCommand = commandLine + " 2>&1";
    FILE* pipe = popen(fullCommand.c_str(), "r");
    if (pipe == nullptr) {
        return false;
    }

    char buffer[512];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        outOutput += buffer;
    }
    outExitCode = pclose(pipe);
    return true;
}

#endif
