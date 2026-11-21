/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include <stdlib.h>
#include <string.h>

typedef struct NullGfxState {
    RtsGfxStats stats;
    uint32_t next_handle;
    int width;
    int height;
} NullGfxState;

static bool null_init(void *state, int width, int height) {
    NullGfxState *s = (NullGfxState *)state;
    s->width = width;
    s->height = height;
    s->next_handle = 1u;
    return true;
}

static void null_shutdown(void *state) {
    free(state);
}

static void null_begin_frame(void *state, int width, int height, RtsVec4f clear_rgba) {
    (void)clear_rgba;
    NullGfxState *s = (NullGfxState *)state;
    s->width = width;
    s->height = height;
    s->stats.frames_begun++;
}

static void null_set_viewport(void *state, int x, int y, int width, int height) {
    (void)x;
    (void)y;
    NullGfxState *s = (NullGfxState *)state;
    s->width = width;
    s->height = height;
}

static void null_end_frame(void *state) {
    NullGfxState *s = (NullGfxState *)state;
    s->stats.frames_ended++;
}

static RtsGfxHandle null_create_shader(void *state, const char *vert_body, const char *frag_body) {
    NullGfxState *s = (NullGfxState *)state;
    if (!vert_body || !frag_body) return RTS_GFX_INVALID;
    s->stats.shader_creates++;
    return s->next_handle++;
}

static RtsGfxHandle null_create_mesh(void *state, const RtsVertex *verts, size_t vert_count, bool dynamic) {
    (void)verts;
    (void)dynamic;
    NullGfxState *s = (NullGfxState *)state;
    if (vert_count == 0 && !dynamic) return RTS_GFX_INVALID;
    s->stats.mesh_creates++;
    return s->next_handle++;
}

static void null_update_mesh(void *state, RtsGfxHandle mesh, const RtsVertex *verts, size_t vert_count) {
    (void)state;
    (void)mesh;
    (void)verts;
    (void)vert_count;
}

static void null_destroy_mesh(void *state, RtsGfxHandle mesh) {
    (void)state;
    (void)mesh;
}

static RtsGfxHandle null_create_texture_rgba(void *state, int width, int height, const uint8_t *rgba, bool nearest) {
    (void)nearest;
    NullGfxState *s = (NullGfxState *)state;
    if (width <= 0 || height <= 0 || !rgba) return RTS_GFX_INVALID;
    s->stats.tex_creates++;
    return s->next_handle++;
}

static void null_draw(void *state, const RtsDrawCmd *cmd) {
    NullGfxState *s = (NullGfxState *)state;
    if (cmd && cmd->shader != RTS_GFX_INVALID && cmd->mesh != RTS_GFX_INVALID) {
        s->stats.draw_calls++;
    }
}

static bool null_capture_png(void *state, const char *out_path, int width, int height) {
    NullGfxState *s = (NullGfxState *)state;
    if (!out_path || width <= 0 || height <= 0) return false;
    s->stats.screenshots_saved++;
    return true;
}

static RtsGfxStats null_get_stats(void *state) {
    NullGfxState *s = (NullGfxState *)state;
    return s->stats;
}

RtsGfxBackend rts_gfx_null_create(void) {
    NullGfxState *s = (NullGfxState *)calloc(1, sizeof(NullGfxState));
    RtsGfxBackend b;
    memset(&b, 0, sizeof(b));
    b.state = s;
    b.init = null_init;
    b.shutdown = null_shutdown;
    b.begin_frame = null_begin_frame;
    b.set_viewport = null_set_viewport;
    b.end_frame = null_end_frame;
    b.create_shader = null_create_shader;
    b.create_mesh = null_create_mesh;
    b.update_mesh = null_update_mesh;
    b.destroy_mesh = null_destroy_mesh;
    b.create_texture_rgba = null_create_texture_rgba;
    b.draw = null_draw;
    b.capture_png = null_capture_png;
    b.get_stats = null_get_stats;
    return b;
}
