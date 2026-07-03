#pragma once

struct NVGcontext;

// Creates the NanoVG context for the current platform backend
// (nanovg_gl on Windows/Linux, MetalNanoVG on Apple platforms).
// Requires a current GL context on the GL path. Returns nullptr on failure.
NVGcontext* CreatePlatformNVGContext();

// Destroys a context created by CreatePlatformNVGContext. Accepts nullptr.
void DestroyPlatformNVGContext(NVGcontext* vg);
