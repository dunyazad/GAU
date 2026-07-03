#include "PlatformConsole.h"

#if defined(_WIN32)

#include <windows.h>

struct LeftmostMonitorSearch
{
    RECT workArea = {};
    bool found = false;
};

static BOOL CALLBACK LeftmostMonitorProc(HMONITOR monitor, HDC hdc, LPRECT rect, LPARAM param)
{
    (void)hdc;
    (void)rect;

    LeftmostMonitorSearch* search = reinterpret_cast<LeftmostMonitorSearch*>(param);
    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfo(monitor, &info)) {
        if (!search->found || info.rcWork.left < search->workArea.left) {
            search->workArea = info.rcWork;
            search->found = true;
        }
    }
    return TRUE;
}

void MoveConsoleToLeftMonitorMaximized()
{
    HWND console = GetConsoleWindow();
    if (console == nullptr) {
        return;
    }

    LeftmostMonitorSearch search;
    EnumDisplayMonitors(nullptr, nullptr, LeftmostMonitorProc,
                        reinterpret_cast<LPARAM>(&search));
    if (!search.found) {
        return;
    }

    // Move onto the leftmost monitor first; SW_MAXIMIZE then maximizes
    // on the monitor that contains the window.
    ShowWindow(console, SW_RESTORE);
    SetWindowPos(console, nullptr, search.workArea.left, search.workArea.top,
                 (search.workArea.right - search.workArea.left) / 2,
                 (search.workArea.bottom - search.workArea.top) / 2,
                 SWP_NOZORDER);
    ShowWindow(console, SW_MAXIMIZE);
}

#else

void MoveConsoleToLeftMonitorMaximized()
{
}

#endif
