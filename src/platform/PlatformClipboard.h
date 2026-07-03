#pragma once

#include <string>

// OS clipboard access (SDL). Used by text editing UIs via main.
std::string GetClipboardText();
void SetClipboardText(const std::string& text);
