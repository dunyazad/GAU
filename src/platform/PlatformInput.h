#pragma once

#include "interaction/EditorInputEvent.h"

union SDL_Event;

// Converts an SDL event into a normalized EditorInputEvent.
// Returns false for SDL events that have no editor-level meaning.
bool TranslateSDLEvent(const SDL_Event& sdlEvent, EditorInputEvent& outEvent);
