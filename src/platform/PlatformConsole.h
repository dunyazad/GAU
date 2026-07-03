#pragma once

// Moves the attached console window to the leftmost monitor and
// maximizes it there (Windows only; no-op elsewhere or when no console
// is attached). Called once at startup so the editor window and the
// console sit side by side on multi-monitor setups.
void MoveConsoleToLeftMonitorMaximized();
