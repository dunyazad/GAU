#pragma once

#include <string>

struct NVGcontext;
struct CommentNode;

// Draws one comment (group) box in canvas space (transform must already
// be applied). When editingTitle is true the title bar shows editingText
// with a caret instead of the stored title. Stateless.
void DrawComment(NVGcontext* vg, const CommentNode& comment,
                 bool editingTitle, const std::string& editingText);
