#include "GLLoader.h"

#include <SDL3/SDL.h>

#define GL_DEFINE_FUNCTION(name) PFn_##name name = nullptr;
GL_FUNCTION_LIST(GL_DEFINE_FUNCTION)
#undef GL_DEFINE_FUNCTION

bool LoadGLFunctions()
{
    bool allLoaded = true;

#define GL_LOAD_FUNCTION(fn) \
    fn = reinterpret_cast<PFn_##fn>(SDL_GL_GetProcAddress(#fn)); \
    if (fn == nullptr) { \
        SDL_Log("GLLoader: missing GL function: %s", #fn); \
        allLoaded = false; \
    }

    GL_FUNCTION_LIST(GL_LOAD_FUNCTION)
#undef GL_LOAD_FUNCTION

    return allLoaded;
}
