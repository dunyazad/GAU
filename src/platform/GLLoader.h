#pragma once

// Minimal OpenGL 3.3 core loader for nanovg_gl and basic frame setup.
// Function pointers are resolved through SDL_GL_GetProcAddress, so no
// external GL loader library is required. This header intentionally
// replaces system GL headers; do not include <GL/gl.h> alongside it.

#include <cstddef>

#if defined(_WIN32)
    #define GLAPIENTRY __stdcall
#else
    #define GLAPIENTRY
#endif

typedef void           GLvoid;
typedef unsigned int   GLenum;
typedef unsigned char  GLboolean;
typedef unsigned int   GLbitfield;
typedef signed char    GLbyte;
typedef short          GLshort;
typedef int            GLint;
typedef int            GLsizei;
typedef unsigned char  GLubyte;
typedef unsigned short GLushort;
typedef unsigned int   GLuint;
typedef float          GLfloat;
typedef char           GLchar;
typedef ptrdiff_t      GLsizeiptr;
typedef ptrdiff_t      GLintptr;

#define GL_FALSE                            0
#define GL_TRUE                             1
#define GL_ZERO                             0
#define GL_ONE                              1
#define GL_NO_ERROR                         0
#define GL_TRIANGLES                        0x0004
#define GL_TRIANGLE_STRIP                   0x0005
#define GL_TRIANGLE_FAN                     0x0006
#define GL_EQUAL                            0x0202
#define GL_NOTEQUAL                         0x0205
#define GL_ALWAYS                           0x0207
#define GL_SRC_COLOR                        0x0300
#define GL_ONE_MINUS_SRC_COLOR              0x0301
#define GL_SRC_ALPHA                        0x0302
#define GL_ONE_MINUS_SRC_ALPHA              0x0303
#define GL_DST_ALPHA                        0x0304
#define GL_ONE_MINUS_DST_ALPHA              0x0305
#define GL_DST_COLOR                        0x0306
#define GL_ONE_MINUS_DST_COLOR              0x0307
#define GL_SRC_ALPHA_SATURATE               0x0308
#define GL_FRONT                            0x0404
#define GL_BACK                             0x0405
#define GL_INVALID_ENUM                     0x0500
#define GL_CCW                              0x0901
#define GL_CULL_FACE                        0x0B44
#define GL_DEPTH_TEST                       0x0B71
#define GL_STENCIL_TEST                     0x0B90
#define GL_BLEND                            0x0BE2
#define GL_SCISSOR_TEST                     0x0C11
#define GL_UNPACK_ROW_LENGTH                0x0CF2
#define GL_UNPACK_SKIP_ROWS                 0x0CF3
#define GL_UNPACK_SKIP_PIXELS               0x0CF4
#define GL_UNPACK_ALIGNMENT                 0x0CF5
#define GL_TEXTURE_2D                       0x0DE1
#define GL_UNSIGNED_BYTE                    0x1401
#define GL_FLOAT                            0x1406
#define GL_RED                              0x1903
#define GL_RGBA                             0x1908
#define GL_LUMINANCE                        0x1909
#define GL_KEEP                             0x1E00
#define GL_INCR                             0x1E02
#define GL_NEAREST                          0x2600
#define GL_LINEAR                           0x2601
#define GL_NEAREST_MIPMAP_NEAREST           0x2700
#define GL_LINEAR_MIPMAP_LINEAR             0x2703
#define GL_TEXTURE_MAG_FILTER               0x2800
#define GL_TEXTURE_MIN_FILTER               0x2801
#define GL_TEXTURE_WRAP_S                   0x2802
#define GL_TEXTURE_WRAP_T                   0x2803
#define GL_REPEAT                           0x2901
#define GL_GENERATE_MIPMAP                  0x8191
#define GL_R8                               0x8229
#define GL_CLAMP_TO_EDGE                    0x812F
#define GL_TEXTURE0                         0x84C0
#define GL_INCR_WRAP                        0x8507
#define GL_DECR_WRAP                        0x8508
#define GL_ARRAY_BUFFER                     0x8892
#define GL_STREAM_DRAW                      0x88E0
#define GL_UNIFORM_BUFFER                   0x8A11
#define GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT  0x8A34
#define GL_FRAGMENT_SHADER                  0x8B30
#define GL_VERTEX_SHADER                    0x8B31
#define GL_COMPILE_STATUS                   0x8B81
#define GL_LINK_STATUS                      0x8B82
#define GL_DEPTH_BUFFER_BIT                 0x00000100
#define GL_STENCIL_BUFFER_BIT               0x00000400
#define GL_COLOR_BUFFER_BIT                 0x00004000

typedef void    (GLAPIENTRY *PFn_glActiveTexture)(GLenum texture);
typedef void    (GLAPIENTRY *PFn_glAttachShader)(GLuint program, GLuint shader);
typedef void    (GLAPIENTRY *PFn_glBindAttribLocation)(GLuint program, GLuint index, const GLchar* name);
typedef void    (GLAPIENTRY *PFn_glBindBuffer)(GLenum target, GLuint buffer);
typedef void    (GLAPIENTRY *PFn_glBindBufferRange)(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size);
typedef void    (GLAPIENTRY *PFn_glBindTexture)(GLenum target, GLuint texture);
typedef void    (GLAPIENTRY *PFn_glBindVertexArray)(GLuint array);
typedef void    (GLAPIENTRY *PFn_glBlendFuncSeparate)(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha);
typedef void    (GLAPIENTRY *PFn_glBufferData)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void    (GLAPIENTRY *PFn_glClear)(GLbitfield mask);
typedef void    (GLAPIENTRY *PFn_glClearColor)(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);
typedef void    (GLAPIENTRY *PFn_glColorMask)(GLboolean red, GLboolean green, GLboolean blue, GLboolean alpha);
typedef void    (GLAPIENTRY *PFn_glCompileShader)(GLuint shader);
typedef GLuint  (GLAPIENTRY *PFn_glCreateProgram)(void);
typedef GLuint  (GLAPIENTRY *PFn_glCreateShader)(GLenum type);
typedef void    (GLAPIENTRY *PFn_glCullFace)(GLenum mode);
typedef void    (GLAPIENTRY *PFn_glDeleteBuffers)(GLsizei n, const GLuint* buffers);
typedef void    (GLAPIENTRY *PFn_glDeleteProgram)(GLuint program);
typedef void    (GLAPIENTRY *PFn_glDeleteShader)(GLuint shader);
typedef void    (GLAPIENTRY *PFn_glDeleteTextures)(GLsizei n, const GLuint* textures);
typedef void    (GLAPIENTRY *PFn_glDeleteVertexArrays)(GLsizei n, const GLuint* arrays);
typedef void    (GLAPIENTRY *PFn_glDisable)(GLenum cap);
typedef void    (GLAPIENTRY *PFn_glDisableVertexAttribArray)(GLuint index);
typedef void    (GLAPIENTRY *PFn_glDrawArrays)(GLenum mode, GLint first, GLsizei count);
typedef void    (GLAPIENTRY *PFn_glEnable)(GLenum cap);
typedef void    (GLAPIENTRY *PFn_glEnableVertexAttribArray)(GLuint index);
typedef void    (GLAPIENTRY *PFn_glFinish)(void);
typedef void    (GLAPIENTRY *PFn_glFrontFace)(GLenum mode);
typedef void    (GLAPIENTRY *PFn_glGenBuffers)(GLsizei n, GLuint* buffers);
typedef void    (GLAPIENTRY *PFn_glGenerateMipmap)(GLenum target);
typedef void    (GLAPIENTRY *PFn_glGenTextures)(GLsizei n, GLuint* textures);
typedef void    (GLAPIENTRY *PFn_glGenVertexArrays)(GLsizei n, GLuint* arrays);
typedef GLenum  (GLAPIENTRY *PFn_glGetError)(void);
typedef void    (GLAPIENTRY *PFn_glGetIntegerv)(GLenum pname, GLint* data);
typedef void    (GLAPIENTRY *PFn_glGetProgramInfoLog)(GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void    (GLAPIENTRY *PFn_glGetProgramiv)(GLuint program, GLenum pname, GLint* params);
typedef void    (GLAPIENTRY *PFn_glGetShaderInfoLog)(GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog);
typedef void    (GLAPIENTRY *PFn_glGetShaderiv)(GLuint shader, GLenum pname, GLint* params);
typedef GLuint  (GLAPIENTRY *PFn_glGetUniformBlockIndex)(GLuint program, const GLchar* uniformBlockName);
typedef GLint   (GLAPIENTRY *PFn_glGetUniformLocation)(GLuint program, const GLchar* name);
typedef void    (GLAPIENTRY *PFn_glLinkProgram)(GLuint program);
typedef void    (GLAPIENTRY *PFn_glPixelStorei)(GLenum pname, GLint param);
typedef void    (GLAPIENTRY *PFn_glShaderSource)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void    (GLAPIENTRY *PFn_glStencilFunc)(GLenum func, GLint ref, GLuint mask);
typedef void    (GLAPIENTRY *PFn_glStencilMask)(GLuint mask);
typedef void    (GLAPIENTRY *PFn_glStencilOp)(GLenum fail, GLenum zfail, GLenum zpass);
typedef void    (GLAPIENTRY *PFn_glStencilOpSeparate)(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass);
typedef void    (GLAPIENTRY *PFn_glTexImage2D)(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pixels);
typedef void    (GLAPIENTRY *PFn_glTexParameteri)(GLenum target, GLenum pname, GLint param);
typedef void    (GLAPIENTRY *PFn_glTexSubImage2D)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void* pixels);
typedef void    (GLAPIENTRY *PFn_glUniform1i)(GLint location, GLint v0);
typedef void    (GLAPIENTRY *PFn_glUniform2fv)(GLint location, GLsizei count, const GLfloat* value);
typedef void    (GLAPIENTRY *PFn_glUniform4fv)(GLint location, GLsizei count, const GLfloat* value);
typedef void    (GLAPIENTRY *PFn_glUniformBlockBinding)(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
typedef void    (GLAPIENTRY *PFn_glUseProgram)(GLuint program);
typedef void    (GLAPIENTRY *PFn_glVertexAttribPointer)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void    (GLAPIENTRY *PFn_glViewport)(GLint x, GLint y, GLsizei width, GLsizei height);

#define GL_FUNCTION_LIST(X) \
    X(glActiveTexture) \
    X(glAttachShader) \
    X(glBindAttribLocation) \
    X(glBindBuffer) \
    X(glBindBufferRange) \
    X(glBindTexture) \
    X(glBindVertexArray) \
    X(glBlendFuncSeparate) \
    X(glBufferData) \
    X(glClear) \
    X(glClearColor) \
    X(glColorMask) \
    X(glCompileShader) \
    X(glCreateProgram) \
    X(glCreateShader) \
    X(glCullFace) \
    X(glDeleteBuffers) \
    X(glDeleteProgram) \
    X(glDeleteShader) \
    X(glDeleteTextures) \
    X(glDeleteVertexArrays) \
    X(glDisable) \
    X(glDisableVertexAttribArray) \
    X(glDrawArrays) \
    X(glEnable) \
    X(glEnableVertexAttribArray) \
    X(glFinish) \
    X(glFrontFace) \
    X(glGenBuffers) \
    X(glGenerateMipmap) \
    X(glGenTextures) \
    X(glGenVertexArrays) \
    X(glGetError) \
    X(glGetIntegerv) \
    X(glGetProgramInfoLog) \
    X(glGetProgramiv) \
    X(glGetShaderInfoLog) \
    X(glGetShaderiv) \
    X(glGetUniformBlockIndex) \
    X(glGetUniformLocation) \
    X(glLinkProgram) \
    X(glPixelStorei) \
    X(glShaderSource) \
    X(glStencilFunc) \
    X(glStencilMask) \
    X(glStencilOp) \
    X(glStencilOpSeparate) \
    X(glTexImage2D) \
    X(glTexParameteri) \
    X(glTexSubImage2D) \
    X(glUniform1i) \
    X(glUniform2fv) \
    X(glUniform4fv) \
    X(glUniformBlockBinding) \
    X(glUseProgram) \
    X(glVertexAttribPointer) \
    X(glViewport)

#define GL_DECLARE_FUNCTION(name) extern PFn_##name name;
GL_FUNCTION_LIST(GL_DECLARE_FUNCTION)
#undef GL_DECLARE_FUNCTION

// Resolves all GL function pointers via SDL_GL_GetProcAddress.
// Requires a current GL context. Returns false if any function is missing.
bool LoadGLFunctions();
