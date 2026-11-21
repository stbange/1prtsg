/* SPDX-License-Identifier: MIT */
#include "gl_loader.h"
#include "rts/gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if RTS_GL_IS_ES
#define GLFN(name) gl##name
#else
#define GLFN(name) rts_gl.name
#endif

#define MAX_GL_RES 128

typedef struct GlMeshRes {
    GLuint vao;
    GLuint vbo;
    GLsizei count;
    bool dynamic;
    bool active;
} GlMeshRes;

typedef struct GlState {
    RtsGfxStats stats;
    GLuint programs[MAX_GL_RES];
    size_t prog_count;
    GlMeshRes meshes[MAX_GL_RES];
    size_t mesh_count;
    GLuint textures[MAX_GL_RES];
    size_t tex_count;
    GLuint white_tex;
} GlState;

static void check_gl_err(GlState *s) {
    GLenum err;
    while ((err = glGetError()) != GL_NO_ERROR) {
        s->stats.gl_errors++;
        fprintf(stderr, "[gfx_gl] OpenGL error: 0x%x\n", (unsigned)err);
    }
}

static bool gl_init(void *state, int width, int height) {
    (void)width;
    (void)height;
    GlState *s = (GlState *)state;
    if (!rts_gl_load_procs()) {
        fprintf(stderr, "[gfx_gl] Failed to load OpenGL 3.3 Core entry points\n");
        return false;
    }
    uint8_t white[4] = {255, 255, 255, 255};
    glGenTextures(1, &s->white_tex);
    glBindTexture(GL_TEXTURE_2D, s->white_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, white);
    check_gl_err(s);
    return true;
}

static void gl_shutdown(void *state) {
    GlState *s = (GlState *)state;
    if (!s) return;
    for (size_t i = 0; i < s->prog_count; ++i) {
        if (s->programs[i]) GLFN(DeleteProgram)(s->programs[i]);
    }
    for (size_t i = 0; i < s->mesh_count; ++i) {
        if (s->meshes[i].active) {
            GLFN(DeleteBuffers)(1, &s->meshes[i].vbo);
            GLFN(DeleteVertexArrays)(1, &s->meshes[i].vao);
        }
    }
    for (size_t i = 0; i < s->tex_count; ++i) {
        if (s->textures[i]) glDeleteTextures(1, &s->textures[i]);
    }
    if (s->white_tex) glDeleteTextures(1, &s->white_tex);
    free(s);
}

static void gl_begin_frame(void *state, int width, int height, RtsVec4f clear_rgba) {
    GlState *s = (GlState *)state;
    glViewport(0, 0, width, height);
    glDepthMask(GL_TRUE);
    glClearColor(clear_rgba.x, clear_rgba.y, clear_rgba.z, clear_rgba.w);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    s->stats.frames_begun++;
}

static void gl_end_frame(void *state) {
    GlState *s = (GlState *)state;
    check_gl_err(s);
    s->stats.frames_ended++;
}

static GLuint compile_stage(GLenum type, const char *body) {
#if RTS_GL_IS_ES
    const char *hdr = "#version 300 es\nprecision highp float;\n";
#else
    const char *hdr = "#version 330 core\n";
#endif
    const GLchar *srcs[2] = {hdr, body};
    GLuint sh = GLFN(CreateShader)(type);
    GLFN(ShaderSource)(sh, 2, srcs, NULL);
    GLFN(CompileShader)(sh);
    GLint ok = 0;
    GLFN(GetShaderiv)(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        GLFN(GetShaderInfoLog)(sh, sizeof(log), NULL, log);
        fprintf(stderr, "[gfx_gl] Shader compile error: %s\n", log);
        GLFN(DeleteShader)(sh);
        return 0;
    }
    return sh;
}

static RtsGfxHandle gl_create_shader(void *state, const char *vert_body, const char *frag_body) {
    GlState *s = (GlState *)state;
    if (!vert_body || !frag_body || s->prog_count >= MAX_GL_RES) return RTS_GFX_INVALID;
    GLuint vs = compile_stage(GL_VERTEX_SHADER, vert_body);
    GLuint fs = compile_stage(GL_FRAGMENT_SHADER, frag_body);
    if (!vs || !fs) {
        if (vs) GLFN(DeleteShader)(vs);
        if (fs) GLFN(DeleteShader)(fs);
        return RTS_GFX_INVALID;
    }
    GLuint prog = GLFN(CreateProgram)();
    GLFN(AttachShader)(prog, vs);
    GLFN(AttachShader)(prog, fs);
    GLFN(LinkProgram)(prog);
    GLFN(DeleteShader)(vs);
    GLFN(DeleteShader)(fs);
    GLint ok = 0;
    GLFN(GetProgramiv)(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[512];
        GLFN(GetProgramInfoLog)(prog, sizeof(log), NULL, log);
        fprintf(stderr, "[gfx_gl] Shader link error: %s\n", log);
        GLFN(DeleteProgram)(prog);
        return RTS_GFX_INVALID;
    }
    s->programs[s->prog_count] = prog;
    s->stats.shader_creates++;
    check_gl_err(s);
    return (RtsGfxHandle)(++s->prog_count);
}

static RtsGfxHandle gl_create_mesh(void *state, const RtsVertex *verts, size_t vert_count, bool dynamic) {
    GlState *s = (GlState *)state;
    if (s->mesh_count >= MAX_GL_RES) return RTS_GFX_INVALID;
    GlMeshRes *m = &s->meshes[s->mesh_count];
    memset(m, 0, sizeof(*m));
    GLFN(GenVertexArrays)(1, &m->vao);
    GLFN(GenBuffers)(1, &m->vbo);
    GLFN(BindVertexArray)(m->vao);
    GLFN(BindBuffer)(GL_ARRAY_BUFFER, m->vbo);
    GLFN(BufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)(vert_count * sizeof(RtsVertex)), verts, dynamic ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    GLsizei stride = (GLsizei)sizeof(RtsVertex);
    GLFN(EnableVertexAttribArray)(0);
    GLFN(VertexAttribPointer)(0, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(RtsVertex, px));
    GLFN(EnableVertexAttribArray)(1);
    GLFN(VertexAttribPointer)(1, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(RtsVertex, nx));
    GLFN(EnableVertexAttribArray)(2);
    GLFN(VertexAttribPointer)(2, 2, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(RtsVertex, u));
    GLFN(EnableVertexAttribArray)(3);
    GLFN(VertexAttribPointer)(3, 3, GL_FLOAT, GL_FALSE, stride, (const void *)offsetof(RtsVertex, r));
    GLFN(BindVertexArray)(0);

    m->count = (GLsizei)vert_count;
    m->dynamic = dynamic;
    m->active = true;
    s->stats.mesh_creates++;
    check_gl_err(s);
    return (RtsGfxHandle)(++s->mesh_count);
}

static void gl_update_mesh(void *state, RtsGfxHandle mesh, const RtsVertex *verts, size_t vert_count) {
    GlState *s = (GlState *)state;
    if (mesh == RTS_GFX_INVALID || mesh > s->mesh_count) return;
    GlMeshRes *m = &s->meshes[mesh - 1u];
    GLFN(BindBuffer)(GL_ARRAY_BUFFER, m->vbo);
    GLFN(BufferData)(GL_ARRAY_BUFFER, (GLsizeiptr)(vert_count * sizeof(RtsVertex)), verts, GL_DYNAMIC_DRAW);
    m->count = (GLsizei)vert_count;
}

static void gl_destroy_mesh(void *state, RtsGfxHandle mesh) {
    GlState *s = (GlState *)state;
    if (mesh == RTS_GFX_INVALID || mesh > s->mesh_count) return;
    GlMeshRes *m = &s->meshes[mesh - 1u];
    if (m->active) {
        GLFN(DeleteBuffers)(1, &m->vbo);
        GLFN(DeleteVertexArrays)(1, &m->vao);
        m->active = false;
    }
}

static RtsGfxHandle gl_create_texture_rgba(void *state, int width, int height, const uint8_t *rgba, bool nearest) {
    GlState *s = (GlState *)state;
    if (s->tex_count >= MAX_GL_RES || width <= 0 || height <= 0 || !rgba) return RTS_GFX_INVALID;
    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    GLint filt = nearest ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filt);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    s->textures[s->tex_count] = tex;
    s->stats.tex_creates++;
    check_gl_err(s);
    return (RtsGfxHandle)(++s->tex_count);
}

static void gl_draw(void *state, const RtsDrawCmd *cmd) {
    GlState *s = (GlState *)state;
    if (!cmd || cmd->shader == RTS_GFX_INVALID || cmd->shader > s->prog_count ||
        cmd->mesh == RTS_GFX_INVALID || cmd->mesh > s->mesh_count) {
        return;
    }
    GlMeshRes *m = &s->meshes[cmd->mesh - 1u];
    if (!m->active || m->count <= 0) return;

    GLuint prog = s->programs[cmd->shader - 1u];
    GLFN(UseProgram)(prog);

    if (cmd->depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    glDepthMask(cmd->depth_write ? GL_TRUE : GL_FALSE);
    if (cmd->blend_additive) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    } else if (cmd->blend_alpha) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }

    GLFN(ActiveTexture)(GL_TEXTURE0);
    GLuint tex = (cmd->texture != RTS_GFX_INVALID && cmd->texture <= s->tex_count)
        ? s->textures[cmd->texture - 1u] : s->white_tex;
    glBindTexture(GL_TEXTURE_2D, tex);

    GLint loc;
    if ((loc = GLFN(GetUniformLocation)(prog, "u_mvp")) >= 0)       GLFN(UniformMatrix4fv)(loc, 1, GL_FALSE, cmd->mvp.m);
    if ((loc = GLFN(GetUniformLocation)(prog, "u_model")) >= 0)     GLFN(UniformMatrix4fv)(loc, 1, GL_FALSE, cmd->model.m);
    if ((loc = GLFN(GetUniformLocation)(prog, "u_tint")) >= 0)      GLFN(Uniform4fv)(loc, 1, &cmd->tint.x);
    if ((loc = GLFN(GetUniformLocation)(prog, "u_light_dir")) >= 0) GLFN(Uniform3fv)(loc, 1, &cmd->light_dir.x);
    if ((loc = GLFN(GetUniformLocation)(prog, "u_time")) >= 0)      GLFN(Uniform1f)(loc, cmd->time_s);
    if ((loc = GLFN(GetUniformLocation)(prog, "u_tex")) >= 0)       GLFN(Uniform1i)(loc, 0);

    GLFN(BindVertexArray)(m->vao);
    glDrawArrays(cmd->mode == RTS_DRAW_LINES ? GL_LINES : GL_TRIANGLES, 0, m->count);
    GLFN(BindVertexArray)(0);
    s->stats.draw_calls++;
}

static bool gl_capture_png(void *state, const char *out_path, int width, int height) {
    GlState *s = (GlState *)state;
    if (!out_path || width <= 0 || height <= 0) return false;
    size_t row_bytes = (size_t)width * 4u;
    uint8_t *raw = (uint8_t *)malloc(row_bytes * (size_t)height);
    uint8_t *flipped = (uint8_t *)malloc(row_bytes * (size_t)height);
    if (!raw || !flipped) {
        free(raw);
        free(flipped);
        return false;
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, raw);
    for (int y = 0; y < height; ++y) {
        memcpy(flipped + (size_t)y * row_bytes, raw + (size_t)(height - 1 - y) * row_bytes, row_bytes);
    }
    bool ok = rts_image_write_png(out_path, width, height, 4, flipped);
    free(raw);
    free(flipped);
    if (ok) s->stats.screenshots_saved++;
    check_gl_err(s);
    return ok;
}

static RtsGfxStats gl_get_stats(void *state) {
    GlState *s = (GlState *)state;
    return s->stats;
}

RtsGfxBackend rts_gfx_gl_create(void) {
    GlState *s = (GlState *)calloc(1, sizeof(GlState));
    RtsGfxBackend b;
    memset(&b, 0, sizeof(b));
    b.state = s;
    b.init = gl_init;
    b.shutdown = gl_shutdown;
    b.begin_frame = gl_begin_frame;
    b.end_frame = gl_end_frame;
    b.create_shader = gl_create_shader;
    b.create_mesh = gl_create_mesh;
    b.update_mesh = gl_update_mesh;
    b.destroy_mesh = gl_destroy_mesh;
    b.create_texture_rgba = gl_create_texture_rgba;
    b.draw = gl_draw;
    b.capture_png = gl_capture_png;
    b.get_stats = gl_get_stats;
    return b;
}
