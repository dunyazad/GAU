#pragma once

// Category-driven exec pin policy (UE style): whether a node takes part
// in the exec flow is decided by its category, never authored by hand.
// - Pure and Object classes carry no exec pins at all.
// - Event classes source the flow: exec output(s) only, no exec input.
// - Every other category gets exactly one unnamed exec input.
// - Authored exec outputs are kept (Branch-style True/False); a flow
//   category without any gets a default "then" output.
// Exec pins always precede the data pins of their direction, so they
// render at the top of the node.

#include "NodeClass.h"

#include <string>
#include <vector>

bool CategoryHasExecFlow(const std::string& category);

std::vector<PinDef> NormalizeClassExecPins(const std::string& category,
                                           std::vector<PinDef> pins);
