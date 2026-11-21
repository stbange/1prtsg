/* SPDX-License-Identifier: MIT */
#ifndef RTS_GFX_H
#define RTS_GFX_H

#include "rts/input.h"
#include "rts/math3d.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint32_t RtsGfxHandle;
#define RTS_GFX_INVALID 0u

typedef struct RtsVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float r, g, b;
} RtsVertex;

typedef enum RtsDrawMode {
    RTS_DRAW_TRIANGLES = 0,
    RTS_DRAW_LINES = 1
} RtsDrawMode;

typedef struct RtsDrawCmd {
    RtsGfxHandle shader;
    RtsGfxHandle mesh;
    RtsGfxHandle texture;
    RtsMat4f mvp;
    RtsMat4f model;
    RtsVec4f tint;
    RtsVec3f light_dir;
    float time_s;
    bool blend_additive;
    bool blend_alpha;
    bool depth_write;
    bool depth_test;
    bool wireframe;
    RtsDrawMode mode;
} RtsDrawCmd;

typedef struct RtsImage {
    int width;
    int height;
    int channels;
    uint8_t *pixels;
} RtsImage;

typedef struct RtsObjMesh {
    RtsVertex *verts;
    size_t vert_count;
} RtsObjMesh;

typedef struct RtsGfxStats {
    uint32_t draw_calls;
    uint32_t shader_creates;
    uint32_t mesh_creates;
    uint32_t tex_creates;
    uint32_t frames_begun;
    uint32_t frames_ended;
    uint32_t screenshots_saved;
    uint32_t gl_errors;
} RtsGfxStats;

typedef struct RtsGfxBackend {
    void *state;
    bool (*init)(void *state, int width, int height);
    void (*shutdown)(void *state);
    void (*begin_frame)(void *state, int width, int height, RtsVec4f clear_rgba);
    void (*end_frame)(void *state);
    RtsGfxHandle (*create_shader)(void *state, const char *vert_body, const char *frag_body);
    RtsGfxHandle (*create_mesh)(void *state, const RtsVertex *verts, size_t vert_count, bool dynamic);
    void (*update_mesh)(void *state, RtsGfxHandle mesh, const RtsVertex *verts, size_t vert_count);
    void (*destroy_mesh)(void *state, RtsGfxHandle mesh);
    RtsGfxHandle (*create_texture_rgba)(void *state, int width, int height, const uint8_t *rgba, bool nearest);
    void (*draw)(void *state, const RtsDrawCmd *cmd);
    bool (*capture_png)(void *state, const char *out_path, int width, int height);
    RtsGfxStats (*get_stats)(void *state);
} RtsGfxBackend;

RtsGfxBackend rts_gfx_null_create(void);
RtsGfxBackend rts_gfx_gl_create(void);

bool rts_image_load_png_memory(const uint8_t *bytes, size_t size, RtsImage *out_img);
void rts_image_free(RtsImage *img);
bool rts_image_write_png(const char *path, int width, int height, int channels, const uint8_t *pixels);

bool rts_obj_load_memory(const char *obj_text, RtsVec3f tint, RtsObjMesh *out_mesh);
void rts_obj_free(RtsObjMesh *mesh);

typedef enum RtsShowcaseScene {
    RTS_SCENE_WAR_ROOM_HOLOGLOBE = 0,
    RTS_SCENE_PLANETARY_SURFACE = 1,
    RTS_SCENE_DROPSHIP_INTERIOR = 2,
    RTS_SCENE_ORBITAL_SHIPYARD = 3,
    RTS_SCENE_MOBILE_TOUCH_HUD = 4,
    RTS_SCENE_COUNT = 5
} RtsShowcaseScene;

typedef struct RtsRetroRenderer {
    RtsGfxBackend gfx;
    const RtsVfs *vfs;
    int width;
    int height;

    RtsGfxHandle sh_mesh;
    RtsGfxHandle sh_shield;
    RtsGfxHandle sh_holo;
    RtsGfxHandle sh_hud;

    RtsGfxHandle tex_hull_blue;
    RtsGfxHandle tex_hull_red;
    RtsGfxHandle tex_deck_floor;
    RtsGfxHandle tex_planet_surface;
    RtsGfxHandle tex_crew_sprite;
    RtsGfxHandle tex_font_atlas;

    RtsGfxHandle mesh_cube;
    RtsGfxHandle mesh_sphere;
    RtsGfxHandle mesh_ring;
    RtsGfxHandle mesh_citadel;
    RtsGfxHandle mesh_dropship;
    RtsGfxHandle mesh_tank;
    RtsGfxHandle mesh_dreadnought;
    RtsGfxHandle mesh_shipyard;
    RtsGfxHandle mesh_starfighter;
    RtsGfxHandle mesh_crew3d;
    RtsGfxHandle mesh_voxel_sample;
    RtsGfxHandle mesh_dynamic_2d;
    RtsGfxHandle mesh_dynamic_3d;

    bool initialized;
} RtsRetroRenderer;

bool rts_renderer_init(RtsRetroRenderer *rr, RtsGfxBackend gfx, const RtsVfs *vfs, int width, int height);
void rts_renderer_shutdown(RtsRetroRenderer *rr);
void rts_renderer_render_scene(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, RtsShowcaseScene scene, float time_s);
bool rts_renderer_capture_png(RtsRetroRenderer *rr, const char *out_path);

#endif /* RTS_GFX_H */
