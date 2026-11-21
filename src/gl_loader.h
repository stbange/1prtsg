/* SPDX-License-Identifier: MIT */
#ifndef RTS_GL_LOADER_H
#define RTS_GL_LOADER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define RTS_GL_IS_ES 1
#else
#define RTS_GL_IS_ES 0
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <GL/gl.h>
#define RTS_APIENTRY APIENTRY
#elif defined(__APPLE__)
#include <OpenGL/gl3.h>
#define RTS_APIENTRY
#else
#include <GL/gl.h>
#define RTS_APIENTRY
#endif

typedef char GLchar;
typedef ptrdiff_t GLsizeiptr;
typedef ptrdiff_t GLintptr;

#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER 0x8B30
#define GL_VERTEX_SHADER 0x8B31
#define GL_COMPILE_STATUS 0x8B81
#define GL_LINK_STATUS 0x8B82
#define GL_ARRAY_BUFFER 0x8892
#define GL_STATIC_DRAW 0x88E4
#define GL_DYNAMIC_DRAW 0x88E8
#define GL_TEXTURE0 0x84C0
#define GL_CLAMP_TO_EDGE 0x812F
#endif

typedef GLuint (RTS_APIENTRY *PFN_glCreateShader)(GLenum);
typedef void   (RTS_APIENTRY *PFN_glShaderSource)(GLuint, GLsizei, const GLchar *const *, const GLint *);
typedef void   (RTS_APIENTRY *PFN_glCompileShader)(GLuint);
typedef void   (RTS_APIENTRY *PFN_glGetShaderiv)(GLuint, GLenum, GLint *);
typedef void   (RTS_APIENTRY *PFN_glGetShaderInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void   (RTS_APIENTRY *PFN_glDeleteShader)(GLuint);
typedef GLuint (RTS_APIENTRY *PFN_glCreateProgram)(void);
typedef void   (RTS_APIENTRY *PFN_glAttachShader)(GLuint, GLuint);
typedef void   (RTS_APIENTRY *PFN_glLinkProgram)(GLuint);
typedef void   (RTS_APIENTRY *PFN_glGetProgramiv)(GLuint, GLenum, GLint *);
typedef void   (RTS_APIENTRY *PFN_glGetProgramInfoLog)(GLuint, GLsizei, GLsizei *, GLchar *);
typedef void   (RTS_APIENTRY *PFN_glUseProgram)(GLuint);
typedef void   (RTS_APIENTRY *PFN_glDeleteProgram)(GLuint);
typedef GLint  (RTS_APIENTRY *PFN_glGetUniformLocation)(GLuint, const GLchar *);
typedef void   (RTS_APIENTRY *PFN_glUniform1i)(GLint, GLint);
typedef void   (RTS_APIENTRY *PFN_glUniform1f)(GLint, GLfloat);
typedef void   (RTS_APIENTRY *PFN_glUniform3fv)(GLint, GLsizei, const GLfloat *);
typedef void   (RTS_APIENTRY *PFN_glUniform4fv)(GLint, GLsizei, const GLfloat *);
typedef void   (RTS_APIENTRY *PFN_glUniformMatrix4fv)(GLint, GLsizei, GLboolean, const GLfloat *);
typedef void   (RTS_APIENTRY *PFN_glGenVertexArrays)(GLsizei, GLuint *);
typedef void   (RTS_APIENTRY *PFN_glBindVertexArray)(GLuint);
typedef void   (RTS_APIENTRY *PFN_glDeleteVertexArrays)(GLsizei, const GLuint *);
typedef void   (RTS_APIENTRY *PFN_glGenBuffers)(GLsizei, GLuint *);
typedef void   (RTS_APIENTRY *PFN_glBindBuffer)(GLenum, GLuint);
typedef void   (RTS_APIENTRY *PFN_glBufferData)(GLenum, GLsizeiptr, const void *, GLenum);
typedef void   (RTS_APIENTRY *PFN_glDeleteBuffers)(GLsizei, const GLuint *);
typedef void   (RTS_APIENTRY *PFN_glEnableVertexAttribArray)(GLuint);
typedef void   (RTS_APIENTRY *PFN_glVertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void *);
typedef void   (RTS_APIENTRY *PFN_glActiveTexture)(GLenum);

typedef struct RtsGlProcs {
    PFN_glCreateShader CreateShader;
    PFN_glShaderSource ShaderSource;
    PFN_glCompileShader CompileShader;
    PFN_glGetShaderiv GetShaderiv;
    PFN_glGetShaderInfoLog GetShaderInfoLog;
    PFN_glDeleteShader DeleteShader;
    PFN_glCreateProgram CreateProgram;
    PFN_glAttachShader AttachShader;
    PFN_glLinkProgram LinkProgram;
    PFN_glGetProgramiv GetProgramiv;
    PFN_glGetProgramInfoLog GetProgramInfoLog;
    PFN_glUseProgram UseProgram;
    PFN_glDeleteProgram DeleteProgram;
    PFN_glGetUniformLocation GetUniformLocation;
    PFN_glUniform1i Uniform1i;
    PFN_glUniform1f Uniform1f;
    PFN_glUniform3fv Uniform3fv;
    PFN_glUniform4fv Uniform4fv;
    PFN_glUniformMatrix4fv UniformMatrix4fv;
    PFN_glGenVertexArrays GenVertexArrays;
    PFN_glBindVertexArray BindVertexArray;
    PFN_glDeleteVertexArrays DeleteVertexArrays;
    PFN_glGenBuffers GenBuffers;
    PFN_glBindBuffer BindBuffer;
    PFN_glBufferData BufferData;
    PFN_glDeleteBuffers DeleteBuffers;
    PFN_glEnableVertexAttribArray EnableVertexAttribArray;
    PFN_glVertexAttribPointer VertexAttribPointer;
    PFN_glActiveTexture ActiveTexture;
} RtsGlProcs;

extern RtsGlProcs rts_gl;
#endif

bool rts_gl_load_procs(void);

#endif /* RTS_GL_LOADER_H */
