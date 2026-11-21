/* SPDX-License-Identifier: MIT */
#include "gl_loader.h"
#include <SDL3/SDL.h>
#include <string.h>

#if !RTS_GL_IS_ES
RtsGlProcs rts_gl;

static bool load_fn(void **dst, const char *name) {
    SDL_FunctionPointer fp = SDL_GL_GetProcAddress(name);
    if (!fp) {
        return false;
    }
    memcpy(dst, &fp, sizeof(void *));
    return true;
}

bool rts_gl_load_procs(void) {
    memset(&rts_gl, 0, sizeof(rts_gl));
    bool ok = true;
    ok &= load_fn((void **)&rts_gl.CreateShader, "glCreateShader");
    ok &= load_fn((void **)&rts_gl.ShaderSource, "glShaderSource");
    ok &= load_fn((void **)&rts_gl.CompileShader, "glCompileShader");
    ok &= load_fn((void **)&rts_gl.GetShaderiv, "glGetShaderiv");
    ok &= load_fn((void **)&rts_gl.GetShaderInfoLog, "glGetShaderInfoLog");
    ok &= load_fn((void **)&rts_gl.DeleteShader, "glDeleteShader");
    ok &= load_fn((void **)&rts_gl.CreateProgram, "glCreateProgram");
    ok &= load_fn((void **)&rts_gl.AttachShader, "glAttachShader");
    ok &= load_fn((void **)&rts_gl.LinkProgram, "glLinkProgram");
    ok &= load_fn((void **)&rts_gl.GetProgramiv, "glGetProgramiv");
    ok &= load_fn((void **)&rts_gl.GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_fn((void **)&rts_gl.UseProgram, "glUseProgram");
    ok &= load_fn((void **)&rts_gl.DeleteProgram, "glDeleteProgram");
    ok &= load_fn((void **)&rts_gl.GetUniformLocation, "glGetUniformLocation");
    ok &= load_fn((void **)&rts_gl.Uniform1i, "glUniform1i");
    ok &= load_fn((void **)&rts_gl.Uniform1f, "glUniform1f");
    ok &= load_fn((void **)&rts_gl.Uniform3fv, "glUniform3fv");
    ok &= load_fn((void **)&rts_gl.Uniform4fv, "glUniform4fv");
    ok &= load_fn((void **)&rts_gl.UniformMatrix4fv, "glUniformMatrix4fv");
    ok &= load_fn((void **)&rts_gl.GenVertexArrays, "glGenVertexArrays");
    ok &= load_fn((void **)&rts_gl.BindVertexArray, "glBindVertexArray");
    ok &= load_fn((void **)&rts_gl.DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_fn((void **)&rts_gl.GenBuffers, "glGenBuffers");
    ok &= load_fn((void **)&rts_gl.BindBuffer, "glBindBuffer");
    ok &= load_fn((void **)&rts_gl.BufferData, "glBufferData");
    ok &= load_fn((void **)&rts_gl.DeleteBuffers, "glDeleteBuffers");
    ok &= load_fn((void **)&rts_gl.EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load_fn((void **)&rts_gl.VertexAttribPointer, "glVertexAttribPointer");
    ok &= load_fn((void **)&rts_gl.ActiveTexture, "glActiveTexture");
    return ok;
}
#else
bool rts_gl_load_procs(void) {
    return true;
}
#endif
