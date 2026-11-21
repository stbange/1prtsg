/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/tactical_sim.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static RtsGfxHandle load_shader_pair(RtsRetroRenderer *rr, const char *vert_path, const char *frag_path) {
    char *vs = rts_vfs_read_text(rr->vfs, "assets", vert_path, NULL);
    char *fs = rts_vfs_read_text(rr->vfs, "assets", frag_path, NULL);
    RtsGfxHandle h = RTS_GFX_INVALID;
    if (vs && fs) {
        h = rr->gfx.create_shader(rr->gfx.state, vs, fs);
    }
    rts_vfs_free(vs);
    rts_vfs_free(fs);
    return h;
}

static RtsGfxHandle load_png_tex_ex(RtsRetroRenderer *rr, const char *png_path, bool nearest) {
    uint8_t *bytes = NULL;
    size_t sz = 0;
    if (!rts_vfs_read_bytes(rr->vfs, "assets", png_path, &bytes, &sz)) {
        return RTS_GFX_INVALID;
    }
    RtsImage img;
    RtsGfxHandle h = RTS_GFX_INVALID;
    if (rts_image_load_png_memory(bytes, sz, &img)) {
        h = rr->gfx.create_texture_rgba(rr->gfx.state, img.width, img.height, img.pixels, nearest);
        rts_image_free(&img);
    }
    rts_vfs_free(bytes);
    return h;
}

static RtsGfxHandle load_obj_mesh(RtsRetroRenderer *rr, const char *obj_path, RtsVec3f tint) {
    char *txt = rts_vfs_read_text(rr->vfs, "assets", obj_path, NULL);
    if (!txt) {
        return RTS_GFX_INVALID;
    }
    RtsObjMesh m;
    RtsGfxHandle h = RTS_GFX_INVALID;
    if (rts_obj_load_memory(txt, tint, &m)) {
        h = rr->gfx.create_mesh(rr->gfx.state, m.verts, m.vert_count, false);
        rts_obj_free(&m);
    }
    rts_vfs_free(txt);
    return h;
}

static RtsGfxHandle build_unit_cube(RtsRetroRenderer *rr) {
    static const float faces[6][4][3] = {
        {{ 0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f, 0.5f}},
        {{-0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f,-0.5f}},
        {{-0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, { 0.5f, 0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}},
        {{-0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f,-0.5f}, { 0.5f,-0.5f, 0.5f}, {-0.5f,-0.5f, 0.5f}},
        {{-0.5f,-0.5f, 0.5f}, { 0.5f,-0.5f, 0.5f}, { 0.5f, 0.5f, 0.5f}, {-0.5f, 0.5f, 0.5f}},
        {{ 0.5f,-0.5f,-0.5f}, {-0.5f,-0.5f,-0.5f}, {-0.5f, 0.5f,-0.5f}, { 0.5f, 0.5f,-0.5f}}
    };
    static const float norms[6][3] = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}
    };
    static const float uvs[4][2] = {{0,0}, {1,0}, {1,1}, {0,1}};
    static const int idx[6] = {0, 1, 2, 0, 2, 3};
    RtsVertex v[36];
    int c = 0;
    for (int f = 0; f < 6; ++f) {
        for (int k = 0; k < 6; ++k) {
            int i = idx[k];
            v[c++] = (RtsVertex){
                faces[f][i][0], faces[f][i][1], faces[f][i][2],
                norms[f][0], norms[f][1], norms[f][2],
                uvs[i][0], uvs[i][1], 1.0f, 1.0f, 1.0f
            };
        }
    }
    return rr->gfx.create_mesh(rr->gfx.state, v, 36, false);
}

static RtsGfxHandle build_uv_sphere(RtsRetroRenderer *rr, int stacks, int slices) {
    size_t max_v = (size_t)(stacks * slices * 6);
    RtsVertex *v = (RtsVertex *)malloc(max_v * sizeof(RtsVertex));
    if (!v) return RTS_GFX_INVALID;
    size_t c = 0;
    for (int i = 0; i < stacks; ++i) {
        float t0 = (float)i / (float)stacks;
        float t1 = (float)(i + 1) / (float)stacks;
        float phi0 = (float)(t0 * RTS_PI);
        float phi1 = (float)(t1 * RTS_PI);
        for (int j = 0; j < slices; ++j) {
            float s0 = (float)j / (float)slices;
            float s1 = (float)(j + 1) / (float)slices;
            float th0 = (float)(s0 * 2.0 * RTS_PI);
            float th1 = (float)(s1 * 2.0 * RTS_PI);
            float p[4][3] = {
                {sinf(phi0) * cosf(th0), cosf(phi0), sinf(phi0) * sinf(th0)},
                {sinf(phi1) * cosf(th0), cosf(phi1), sinf(phi1) * sinf(th0)},
                {sinf(phi1) * cosf(th1), cosf(phi1), sinf(phi1) * sinf(th1)},
                {sinf(phi0) * cosf(th1), cosf(phi0), sinf(phi0) * sinf(th1)}
            };
            float uv[4][2] = {{s0, t0}, {s0, t1}, {s1, t1}, {s1, t0}};
            static const int tri[6] = {0, 1, 2, 0, 2, 3};
            for (int k = 0; k < 6; ++k) {
                int q = tri[k];
                v[c++] = (RtsVertex){p[q][0], p[q][1], p[q][2], p[q][0], p[q][1], p[q][2], uv[q][0], uv[q][1], 1.0f, 1.0f, 1.0f};
            }
        }
    }
    RtsGfxHandle h = rr->gfx.create_mesh(rr->gfx.state, v, c, false);
    free(v);
    return h;
}

static RtsGfxHandle build_orbit_ring(RtsRetroRenderer *rr, int segs) {
    size_t max_v = (size_t)(segs * 2);
    RtsVertex *v = (RtsVertex *)malloc(max_v * sizeof(RtsVertex));
    if (!v) return RTS_GFX_INVALID;
    size_t c = 0;
    for (int i = 0; i < segs; ++i) {
        float a0 = (float)((double)i / (double)segs * 2.0 * RTS_PI);
        float a1 = (float)((double)(i + 1) / (double)segs * 2.0 * RTS_PI);
        v[c++] = (RtsVertex){cosf(a0), 0.0f, sinf(a0), 0, 1, 0, 0, 0, 1, 1, 1};
        v[c++] = (RtsVertex){cosf(a1), 0.0f, sinf(a1), 0, 1, 0, 1, 0, 1, 1, 1};
    }
    RtsGfxHandle h = rr->gfx.create_mesh(rr->gfx.state, v, c, false);
    free(v);
    return h;
}

static RtsGfxHandle load_voxel_sample(RtsRetroRenderer *rr, const char *vxl_path) {
    char *txt = rts_vfs_read_text(rr->vfs, "assets", vxl_path, NULL);
    if (!txt) return RTS_GFX_INVALID;
    RtsVoxelChunk chunk;
    rts_voxel_init(&chunk, 0.8f);
    RtsGfxHandle h = RTS_GFX_INVALID;
    if (rts_voxel_load_text(&chunk, txt)) {
        RtsVoxelVertex *vv = (RtsVoxelVertex *)malloc(8192 * sizeof(RtsVoxelVertex));
        RtsVertex *rv = (RtsVertex *)malloc(8192 * sizeof(RtsVertex));
        if (vv && rv) {
            size_t n = rts_voxel_build_mesh(&chunk, vv, 8192);
            for (size_t i = 0; i < n; ++i) {
                rv[i] = (RtsVertex){vv[i].px, vv[i].py, vv[i].pz, vv[i].nx, vv[i].ny, vv[i].nz, vv[i].u, vv[i].v, vv[i].r, vv[i].g, vv[i].b};
            }
            if (n > 0) {
                h = rr->gfx.create_mesh(rr->gfx.state, rv, n, false);
            }
        }
        free(vv);
        free(rv);
    }
    rts_vfs_free(txt);
    return h;
}

bool rts_renderer_init(RtsRetroRenderer *rr, RtsGfxBackend gfx, const RtsVfs *vfs, int width, int height) {
    if (!rr || !vfs || !gfx.init) {
        return false;
    }
    memset(rr, 0, sizeof(*rr));
    rr->gfx = gfx;
    rr->vfs = vfs;
    rr->width = width > 0 ? width : 1280;
    rr->height = height > 0 ? height : 720;

    if (!rr->gfx.init(rr->gfx.state, rr->width, rr->height)) {
        return false;
    }

    rr->sh_mesh   = load_shader_pair(rr, "shaders/mesh.vert",   "shaders/mesh.frag");
    rr->sh_sky    = load_shader_pair(rr, "shaders/sky.vert",    "shaders/sky.frag");
    rr->sh_shield = load_shader_pair(rr, "shaders/shield.vert", "shaders/shield.frag");
    rr->sh_holo   = load_shader_pair(rr, "shaders/holo.vert",   "shaders/holo.frag");
    rr->sh_hud    = load_shader_pair(rr, "shaders/hud.vert",    "shaders/hud.frag");

    /* Smooth crisp filtering on 256/512px sculpted surface textures; nearest on bitmap font & crisp crew sprites */
    rr->tex_hull_blue      = load_png_tex_ex(rr, "textures/hull_blue.png",      false);
    rr->tex_hull_red       = load_png_tex_ex(rr, "textures/hull_red.png",       false);
    rr->tex_deck_floor     = load_png_tex_ex(rr, "textures/deck_floor.png",     false);
    rr->tex_planet_surface = load_png_tex_ex(rr, "textures/planet_surface.png", false);
    rr->tex_ocean          = load_png_tex_ex(rr, "textures/ocean_wave.png",     false);
    rr->tex_crt_console    = load_png_tex_ex(rr, "textures/crt_console.png",    false);
    rr->tex_crew_sprite    = load_png_tex_ex(rr, "sprites/crew_8dir.png",       true);
    rr->tex_font_atlas     = load_png_tex_ex(rr, "fonts/retro_font.png",        true);

    rr->mesh_cube   = build_unit_cube(rr);
    rr->mesh_sphere = build_uv_sphere(rr, 22, 36);
    rr->mesh_ring   = build_orbit_ring(rr, 112);

    rr->mesh_citadel     = load_obj_mesh(rr, "models/citadel.obj",        (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_dropship    = load_obj_mesh(rr, "models/dropship.obj",       (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_tank        = load_obj_mesh(rr, "models/plasma_tank.obj",    (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_dreadnought = load_obj_mesh(rr, "models/dreadnought.obj",    (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_shipyard    = load_obj_mesh(rr, "models/shipyard.obj",       (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_starfighter = load_obj_mesh(rr, "models/starfighter.obj",    (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_carrier     = load_obj_mesh(rr, "models/carrier.obj",        (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_crew3d      = load_obj_mesh(rr, "models/crew_3d.obj",        (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_arch        = load_obj_mesh(rr, "models/interior_arch.obj",  (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_terrain     = load_obj_mesh(rr, "models/terrain_island.obj", (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_voxel_sample = load_voxel_sample(rr, "voxels/bunker_module.vxl");

    rr->mesh_dynamic_2d = rr->gfx.create_mesh(rr->gfx.state, NULL, 0, true);
    rr->mesh_dynamic_3d = rr->gfx.create_mesh(rr->gfx.state, NULL, 0, true);
    rr->initialized = (rr->sh_mesh != RTS_GFX_INVALID && rr->mesh_cube != RTS_GFX_INVALID);
    return rr->initialized;
}

void rts_renderer_shutdown(RtsRetroRenderer *rr) {
    if (!rr || !rr->gfx.shutdown) return;
    rr->gfx.shutdown(rr->gfx.state);
    rr->gfx.state = NULL;
    rr->initialized = false;
}

static void draw_mesh(RtsRetroRenderer *rr, RtsGfxHandle sh, RtsGfxHandle mesh, RtsGfxHandle tex,
                      RtsMat4f vp, RtsMat4f model, RtsVec4f tint, RtsVec3f light_dir,
                      float time_s, bool alpha, bool additive, bool depth_write, bool wireframe, RtsDrawMode mode) {
    if (sh == RTS_GFX_INVALID || mesh == RTS_GFX_INVALID) return;
    RtsDrawCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.shader = sh;
    cmd.mesh = mesh;
    cmd.texture = tex;
    cmd.model = model;
    cmd.mvp = rts_mat4_mul(vp, model);
    cmd.tint = tint;
    cmd.light_dir = light_dir;
    cmd.cam_pos = rr->cur_cam_pos;
    cmd.fog_color = rr->cur_fog_color;
    cmd.time_s = time_s;
    cmd.blend_alpha = alpha;
    cmd.blend_additive = additive;
    cmd.depth_write = depth_write;
    cmd.depth_test = true;
    cmd.wireframe = wireframe;
    cmd.mode = mode;
    rr->gfx.draw(rr->gfx.state, &cmd);
}

static void draw_box(RtsRetroRenderer *rr, RtsMat4f vp, RtsVec3f pos, RtsVec3f scale, RtsQuatd rot,
                     RtsGfxHandle tex, RtsVec4f tint, RtsVec3f light, float t) {
    RtsMat4f m = rts_mat4_from_quat_trs(rot, pos, scale);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_cube, tex, vp, m, tint, light, t, false, false, true, false, RTS_DRAW_TRIANGLES);
}

static void draw_sky_dome(RtsRetroRenderer *rr, RtsMat4f vp, RtsVec3f cam_pos, RtsVec3f sun_dir, float sky_mode, float t) {
    if (rr->sh_sky == RTS_GFX_INVALID) return;
    RtsMat4f m = rts_mat4_from_quat_trs(rts_quat_identity(), cam_pos, (RtsVec3f){640.0f, 640.0f, 640.0f});
    draw_mesh(rr, rr->sh_sky, rr->mesh_sphere, RTS_GFX_INVALID, vp, m,
              (RtsVec4f){sky_mode, 0.0f, 0.0f, 1.0f}, sun_dir, t, false, false, false, false, RTS_DRAW_TRIANGLES);
}

static void draw_billboard_sprite(RtsRetroRenderer *rr, RtsMat4f vp, RtsVec3f pos, RtsVec3f cam_pos,
                                  float w, float h, int dir_idx, int dir_count, RtsGfxHandle tex, RtsVec4f tint, float t) {
    RtsVec3d to_cam = rts_v3d_norm(rts_v3d(cam_pos.x - pos.x, 0.0, cam_pos.z - pos.z));
    RtsVec3d up = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d right = rts_v3d_norm(rts_v3d_cross(up, to_cam));
    float hw = w * 0.5f;
    float u0 = (float)dir_idx / (float)(dir_count > 0 ? dir_count : 8);
    float u1 = (float)(dir_idx + 1) / (float)(dir_count > 0 ? dir_count : 8);

    RtsVec3f p0 = {pos.x - (float)right.x * hw, pos.y,     pos.z - (float)right.z * hw};
    RtsVec3f p1 = {pos.x + (float)right.x * hw, pos.y,     pos.z + (float)right.z * hw};
    RtsVec3f p2 = {pos.x + (float)right.x * hw, pos.y + h, pos.z + (float)right.z * hw};
    RtsVec3f p3 = {pos.x - (float)right.x * hw, pos.y + h, pos.z - (float)right.z * hw};

    RtsVertex v[6] = {
        {p0.x, p0.y, p0.z, 0,1,0, u0, 1.0f, tint.x, tint.y, tint.z},
        {p1.x, p1.y, p1.z, 0,1,0, u1, 1.0f, tint.x, tint.y, tint.z},
        {p2.x, p2.y, p2.z, 0,1,0, u1, 0.0f, tint.x, tint.y, tint.z},
        {p0.x, p0.y, p0.z, 0,1,0, u0, 1.0f, tint.x, tint.y, tint.z},
        {p2.x, p2.y, p2.z, 0,1,0, u1, 0.0f, tint.x, tint.y, tint.z},
        {p3.x, p3.y, p3.z, 0,1,0, u0, 0.0f, tint.x, tint.y, tint.z}
    };
    rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, v, 6);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dynamic_3d, tex, vp, rts_mat4_identity(), tint, (RtsVec3f){0.4f, 0.8f, 0.4f}, t, true, false, true, false, RTS_DRAW_TRIANGLES);
}

static void draw_plasma_beam(RtsRetroRenderer *rr, RtsMat4f vp, RtsVec3f a, RtsVec3f b, float thickness, RtsVec4f col, float t) {
    RtsVec3d diff = rts_v3d(b.x - a.x, b.y - a.y, b.z - a.z);
    double len = rts_v3d_len(diff);
    if (len < 0.01) return;
    RtsVec3d dir = rts_v3d_scale(diff, 1.0 / len);
    RtsVec3d side = rts_v3d_norm(rts_v3d_cross(dir, rts_v3d(0.0, 1.0, 0.0)));
    if (rts_v3d_len_sq(side) < 0.1) side = rts_v3d(1.0, 0.0, 0.0);
    RtsVec3d up = rts_v3d_cross(side, dir);
    RtsVec3f sx = {(float)(side.x * thickness), (float)(side.y * thickness), (float)(side.z * thickness)};
    RtsVec3f sy = {(float)(up.x * thickness),   (float)(up.y * thickness),   (float)(up.z * thickness)};

    RtsVertex v[12] = {
        {a.x - sx.x, a.y - sx.y, a.z - sx.z, 0,1,0, 0,0, col.x,col.y,col.z},
        {a.x + sx.x, a.y + sx.y, a.z + sx.z, 0,1,0, 1,0, col.x,col.y,col.z},
        {b.x + sx.x, b.y + sx.y, b.z + sx.z, 0,1,0, 1,1, col.x,col.y,col.z},
        {a.x - sx.x, a.y - sx.y, a.z - sx.z, 0,1,0, 0,0, col.x,col.y,col.z},
        {b.x + sx.x, b.y + sx.y, b.z + sx.z, 0,1,0, 1,1, col.x,col.y,col.z},
        {b.x - sx.x, b.y - sx.y, b.z - sx.z, 0,1,0, 0,1, col.x,col.y,col.z},

        {a.x - sy.x, a.y - sy.y, a.z - sy.z, 0,1,0, 0,0, col.x,col.y,col.z},
        {a.x + sy.x, a.y + sy.y, a.z + sy.z, 0,1,0, 1,0, col.x,col.y,col.z},
        {b.x + sy.x, b.y + sy.y, b.z + sy.z, 0,1,0, 1,1, col.x,col.y,col.z},
        {a.x - sy.x, a.y - sy.y, a.z - sy.z, 0,1,0, 0,0, col.x,col.y,col.z},
        {b.x + sy.x, b.y + sy.y, b.z + sy.z, 0,1,0, 1,1, col.x,col.y,col.z},
        {b.x - sy.x, b.y - sy.y, b.z - sy.z, 0,1,0, 0,1, col.x,col.y,col.z}
    };
    rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, v, 12);
    draw_mesh(rr, rr->sh_holo, rr->mesh_dynamic_3d, RTS_GFX_INVALID, vp, rts_mat4_identity(), col, (RtsVec3f){0,1,0}, t, false, true, false, false, RTS_DRAW_TRIANGLES);
}

/* --- 2D Sleek Frosted-Glass Sci-Fi HUD & Bitmap Text Helpers --- */
static void hud_quad(RtsRetroRenderer *rr, RtsMat4f ortho, float x, float y, float w, float h, RtsVec4f col) {
    RtsVertex v[6] = {
        {x,     y,     0, 0,0,1, 0,0, col.x,col.y,col.z},
        {x + w, y,     0, 0,0,1, 1,0, col.x,col.y,col.z},
        {x + w, y + h, 0, 0,0,1, 1,1, col.x,col.y,col.z},
        {x,     y,     0, 0,0,1, 0,0, col.x,col.y,col.z},
        {x + w, y + h, 0, 0,0,1, 1,1, col.x,col.y,col.z},
        {x,     y + h, 0, 0,0,1, 0,1, col.x,col.y,col.z}
    };
    rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_2d, v, 6);
    RtsDrawCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.shader = rr->sh_hud;
    cmd.mesh = rr->mesh_dynamic_2d;
    cmd.texture = RTS_GFX_INVALID;
    cmd.mvp = ortho;
    cmd.model = rts_mat4_identity();
    cmd.tint = col;
    cmd.blend_alpha = true;
    cmd.depth_test = false;
    cmd.depth_write = false;
    rr->gfx.draw(rr->gfx.state, &cmd);
}

static void hud_border_box(RtsRetroRenderer *rr, RtsMat4f ortho, float x, float y, float w, float h, RtsVec4f fill, RtsVec4f edge) {
    /* Sleek frosted obsidian-glass panel with razor hairline border + champagne-gold corner ticks */
    hud_quad(rr, ortho, x, y, w, h, fill);
    hud_quad(rr, ortho, x, y, w, 1.5f, edge);
    hud_quad(rr, ortho, x, y + h - 1.5f, w, 1.5f, edge);
    hud_quad(rr, ortho, x, y, 1.5f, h, edge);
    hud_quad(rr, ortho, x + w - 1.5f, y, 1.5f, h, edge);
    RtsVec4f gold = {0.96f, 0.78f, 0.34f, 0.95f};
    float tk = 14.0f;
    hud_quad(rr, ortho, x, y, tk, 3.0f, gold);
    hud_quad(rr, ortho, x, y, 3.0f, tk, gold);
    hud_quad(rr, ortho, x + w - tk, y + h - 3.0f, tk, 3.0f, gold);
    hud_quad(rr, ortho, x + w - 3.0f, y + h - tk, 3.0f, tk, gold);
}

static void hud_text(RtsRetroRenderer *rr, RtsMat4f ortho, float x, float y, float scale, RtsVec4f col, const char *text) {
    if (!text || rr->tex_font_atlas == RTS_GFX_INVALID) return;
    RtsVertex verts[128 * 6];
    size_t c = 0;
    float cx = x;
    float gw = 8.0f * scale;
    float gh = 8.0f * scale;
    for (size_t i = 0; text[i] && c + 6 <= 128 * 6; ++i) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 32 || ch > 127) ch = '?';
        int idx = ch - 32;
        int col_i = idx % 16;
        int row_i = idx / 16;
        float u0 = (float)col_i / 16.0f;
        float u1 = (float)(col_i + 1) / 16.0f;
        float v0 = (float)row_i / 6.0f;
        float v1 = (float)(row_i + 1) / 6.0f;
        verts[c++] = (RtsVertex){cx,      y,      0, 0,0,1, u0, v0, col.x,col.y,col.z};
        verts[c++] = (RtsVertex){cx + gw, y,      0, 0,0,1, u1, v0, col.x,col.y,col.z};
        verts[c++] = (RtsVertex){cx + gw, y + gh, 0, 0,0,1, u1, v1, col.x,col.y,col.z};
        verts[c++] = (RtsVertex){cx,      y,      0, 0,0,1, u0, v0, col.x,col.y,col.z};
        verts[c++] = (RtsVertex){cx + gw, y + gh, 0, 0,0,1, u1, v1, col.x,col.y,col.z};
        verts[c++] = (RtsVertex){cx,      y + gh, 0, 0,0,1, u0, v1, col.x,col.y,col.z};
        cx += gw * 0.92f;
    }
    if (c == 0) return;
    rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_2d, verts, c);
    RtsDrawCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.shader = rr->sh_hud;
    cmd.mesh = rr->mesh_dynamic_2d;
    cmd.texture = rr->tex_font_atlas;
    cmd.mvp = ortho;
    cmd.model = rts_mat4_identity();
    cmd.tint = col;
    cmd.blend_alpha = true;
    cmd.depth_test = false;
    cmd.depth_write = false;
    rr->gfx.draw(rr->gfx.state, &cmd);
}

static void render_hud_common(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input,
                              const char *location_banner, const char *subtitle, const char *radio_line, bool force_touch) {
    float w = (float)rr->width;
    float h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    /* Sleek Frosted Midnight-Glass Command Header */
    hud_border_box(rr, ortho, 16.0f, 12.0f, w - 32.0f, 48.0f,
                   (RtsVec4f){0.03f, 0.06f, 0.13f, 0.72f}, (RtsVec4f){0.22f, 0.88f, 0.98f, 0.85f});
    hud_text(rr, ortho, 28.0f, 20.0f, 1.38f, (RtsVec4f){0.86f, 0.96f, 1.0f, 1.0f}, location_banner);
    hud_text(rr, ortho, 28.0f, 40.0f, 1.08f, (RtsVec4f){0.96f, 0.78f, 0.36f, 1.0f}, subtitle);

    char roster_buf[96];
    size_t bp_n = world ? world->blueprint_count : 76u;
    size_t role_n = world ? world->crew_role_count : 12u;
    snprintf(roster_buf, sizeof(roster_buf), "IRONMAN: ON | ROSTER: %zu UNITS / %zu ROLES", bp_n, role_n);
    hud_text(rr, ortho, w - 452.0f, 20.0f, 1.22f, (RtsVec4f){1.0f, 0.42f, 0.52f, 1.0f}, roster_buf);
    hud_text(rr, ortho, w - 452.0f, 40.0f, 1.08f, (RtsVec4f){0.42f, 0.96f, 0.88f, 1.0f}, "ORE:2500 FUEL:1200 PWR:+400MW SHIELD:100%");

    /* Minimal Precision 1st-Person Reticle */
    float cx = w * 0.5f, cy = h * 0.5f;
    hud_quad(rr, ortho, cx - 9.0f, cy - 0.75f, 5.0f, 1.5f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.80f});
    hud_quad(rr, ortho, cx + 4.0f, cy - 0.75f, 5.0f, 1.5f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.80f});
    hud_quad(rr, ortho, cx - 0.75f, cy - 9.0f, 1.5f, 5.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.80f});
    hud_quad(rr, ortho, cx - 0.75f, cy + 4.0f, 1.5f, 5.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.80f});

    /* Sleek Bottom Radio Comms Strip */
    if (!force_touch) {
        hud_border_box(rr, ortho, 16.0f, h - 48.0f, w - 32.0f, 34.0f,
                       (RtsVec4f){0.03f, 0.06f, 0.13f, 0.72f}, (RtsVec4f){0.96f, 0.78f, 0.34f, 0.80f});
        hud_text(rr, ortho, 28.0f, h - 37.0f, 1.32f, (RtsVec4f){0.94f, 0.97f, 1.0f, 1.0f}, radio_line);
    }

    /* Mobile Web Auto-Touch Virtual Controls Overlay */
    bool show_touch = force_touch || (input && input->mode == RTS_INPUT_MOBILE_TOUCH);
    if (show_touch) {
        RtsTouchLayout tl;
        if (input) {
            tl = input->touch_layout;
        } else {
            RtsInputRouter tmp;
            rts_input_init(&tmp, w, h, true);
            tl = tmp.touch_layout;
        }
        float r = tl.stick_radius;
        hud_border_box(rr, ortho, tl.stick_cx - r, tl.stick_cy - r, r * 2.0f, r * 2.0f,
                       (RtsVec4f){0.04f, 0.12f, 0.24f, 0.44f}, (RtsVec4f){0.25f, 0.90f, 1.0f, 0.90f});
        float kx = input ? input->stick_knob_x : (tl.stick_cx + r * 0.35f);
        float ky = input ? input->stick_knob_y : (tl.stick_cy - r * 0.25f);
        hud_border_box(rr, ortho, kx - 24.0f, ky - 24.0f, 48.0f, 48.0f,
                       (RtsVec4f){0.20f, 0.84f, 0.98f, 0.72f}, (RtsVec4f){0.98f, 0.84f, 0.36f, 1.0f});
        hud_text(rr, ortho, tl.stick_cx - 42.0f, tl.stick_cy - r - 18.0f, 1.35f, (RtsVec4f){0.42f, 0.96f, 1.0f, 1.0f}, "MOVE STICK");

        for (int i = 0; i < RTS_TBTN_COUNT; ++i) {
            RtsRectf rc = tl.buttons[i];
            RtsVec4f edge = (i == RTS_TBTN_INTERACT) ? (RtsVec4f){0.98f, 0.80f, 0.34f, 0.95f} : (RtsVec4f){0.25f, 0.90f, 1.0f, 0.88f};
            hud_border_box(rr, ortho, rc.x, rc.y, rc.w, rc.h, (RtsVec4f){0.04f, 0.10f, 0.22f, 0.62f}, edge);
            hud_text(rr, ortho, rc.x + 16.0f, rc.y + rc.h * 0.36f, 1.55f, (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, tl.labels[i]);
        }

        hud_border_box(rr, ortho, w * 0.30f, h - 48.0f, w * 0.38f, 34.0f,
                       (RtsVec4f){0.03f, 0.08f, 0.18f, 0.78f}, (RtsVec4f){0.30f, 0.96f, 0.78f, 0.92f});
        hud_text(rr, ortho, w * 0.315f, h - 36.0f, 1.30f, (RtsVec4f){0.45f, 1.0f, 0.84f, 1.0f}, "AUTO-TOUCH ACTIVE | PINCH/TAP HOLO-GLOBE");
    }
}

void rts_renderer_render_scene(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, RtsShowcaseScene scene, float time_s) {
    if (!rr || !rr->initialized) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    RtsMat4f proj = rts_mat4_perspective((float)(60.0 * RTS_DEG2RAD), aspect, 0.2f, 1600.0f);
    RtsVec3f sun = {0.62f, 0.36f, -0.70f};
    RtsVec3f light = {0.58f, 0.78f, 0.38f};
    RtsQuatd idq = rts_quat_identity();

    if (scene == RTS_SCENE_WAR_ROOM_HOLOGLOBE || scene == RTS_SCENE_MOBILE_TOUCH_HUD) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.03f, 0.05f, 0.12f, 1.0f});
        RtsVec3f cam = {0.0f, 2.35f, 6.4f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.10f, 0.26f, 0.42f, 0.22f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 1.75f, -1.6f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        /* Solaris Twilight Ring Sky visible through panoramic vaulted War Room canopy */
        draw_sky_dome(rr, vp, cam, sun, 0.0f, time_s);

        /* Sleek Deck Floor + Sweeping Vaulted Cathedral Arches (zero boxy walls!) */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.25f, -1.0f}, (RtsVec3f){22.0f, 0.5f, 22.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s);
        for (int i = 0; i < 4; ++i) {
            float az = 3.2f - (float)i * 4.4f;
            draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_blue, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.0f, az}, (RtsVec3f){0.96f, 0.76f, 1.0f}),
                      (RtsVec4f){0.85f, 0.92f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
        /* Sculpted Lower Panoramic Canopy Sill */
        draw_box(rr, vp, (RtsVec3f){0.0f, 0.45f, -10.4f}, (RtsVec3f){20.0f, 0.9f, 0.55f}, idq, rr->tex_hull_blue, (RtsVec4f){0.55f, 0.68f, 0.88f, 1.0f}, light, time_s);

        /* Sculpted Aerospace Dropship & Obsidian Crimson Dreadnought Outside Panoramic Viewport */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.55), (RtsVec3f){-14.0f, 6.2f, -38.0f}, (RtsVec3f){1.6f, 1.6f, 1.6f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.45), (RtsVec3f){22.0f, 12.5f, -72.0f}, (RtsVec3f){2.2f, 2.2f, 2.2f}),
                  (RtsVec4f){1.0f, 0.92f, 0.95f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sculpted Circular Holo-Globe Pedestal & Angled CRT Tactical Consoles */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.15f, -0.8f}, (RtsVec3f){2.15f, 0.78f, 2.15f}),
                  (RtsVec4f){0.52f, 0.66f, 0.86f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.88f, -0.8f}, (RtsVec3f){1.95f, 1.95f, 1.95f}),
                  (RtsVec4f){0.98f, 0.80f, 0.30f, 0.95f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_box(rr, vp, (RtsVec3f){-5.4f, 0.95f, -1.8f}, (RtsVec3f){2.2f, 1.35f, 0.35f},
                 rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.72), rr->tex_crt_console, (RtsVec4f){1,1,1,1}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 5.4f, 0.95f, -1.8f}, (RtsVec3f){2.2f, 1.35f, 0.35f},
                 rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.72), rr->tex_crt_console, (RtsVec4f){1,1,1,1}, light, time_s);

        /* Sleek Sculpted 3D Officers Staffing the War Room */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.78), (RtsVec3f){-3.4f, 0.0f, -2.0f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.82), (RtsVec3f){ 3.4f, 0.0f, -1.7f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 0.98f, 0.94f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Radiant 3D Planetary & Orbital Holo-Globe with Hex Energy Shell & Keplerian Tracks */
        RtsQuatd spin = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 0.35 + 0.7);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.30f, -0.8f}, (RtsVec3f){1.20f, 1.20f, 1.20f}),
                  (RtsVec4f){0.55f, 0.95f, 1.0f, 0.92f}, light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.30f, -0.8f}, (RtsVec3f){1.30f, 1.30f, 1.30f}),
                  (RtsVec4f){0.18f, 0.92f, 1.0f, 0.58f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        RtsQuatd orb1 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.35), rts_quat_from_axis_angle(rts_v3d(0,0,1), 0.22));
        RtsQuatd orb2 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), -0.52), rts_quat_from_axis_angle(rts_v3d(0,0,1), -0.45));
        RtsQuatd orb3 = rts_quat_from_axis_angle(rts_v3d(1,0,0), 1.05);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb1, (RtsVec3f){0.0f, 2.30f, -0.8f}, (RtsVec3f){1.72f, 1.72f, 1.72f}), (RtsVec4f){1.0f, 0.84f, 0.30f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb2, (RtsVec3f){0.0f, 2.30f, -0.8f}, (RtsVec3f){2.12f, 2.12f, 1.92f}), (RtsVec4f){1.0f, 0.28f, 0.44f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb3, (RtsVec3f){0.0f, 2.30f, -0.8f}, (RtsVec3f){1.50f, 1.50f, 1.50f}), (RtsVec4f){0.25f, 1.0f, 0.82f, 0.92f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

        /* Miniature Glowing Fleet Markers on the Holo-Globe */
        draw_mesh(rr, rr->sh_holo, rr->mesh_starfighter, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(idq, (RtsVec3f){1.60f, 2.55f, -0.45f}, (RtsVec3f){0.08f, 0.08f, 0.08f}), (RtsVec4f){1.0f, 0.88f, 0.32f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_dreadnought, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(idq, (RtsVec3f){-1.85f, 2.80f, -1.10f}, (RtsVec3f){0.06f, 0.06f, 0.06f}), (RtsVec4f){1.0f, 0.25f, 0.42f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_TRIANGLES);

        bool is_touch = (scene == RTS_SCENE_MOBILE_TOUCH_HUD);
        render_hud_common(rr, world, input,
                          is_touch ? "1STPRTS [MOBILE AUTO-TOUCH] - WAR ROOM" : "SOLARIS COMMAND CITADEL - WAR ROOM [HOLO-GLOBE]",
                          "SOLAR DIRECTORATE | HELIOS PRIME (R=6371KM) | KEPLERIAN ORBITS",
                          "XO: ORBITAL SHIPYARD ALPHA PASSING OVERHEAD IN 400KM LEO. CRIMSON CRUISER DETECTED!",
                          is_touch);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_PLANETARY_SURFACE) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.08f, 0.22f, 0.36f, 1.0f});
        RtsVec3f cam = {-26.0f, 8.2f, 44.0f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.36f, 0.44f, 0.56f, 0.85f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){8.0f, 13.0f, -24.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        /* Iconic Solaris Twilight Sky Dome with Sunset Coral Horizon & Sweeping Planetary Ring Arc */
        draw_sky_dome(rr, vp, cam, sun, 0.0f, time_s);

        /* Sculpted Helios Prime Peninsula & Mountain Ridges (clean stylized vertex-shaded low-poly!) + Turquoise Ocean */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_terrain, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){-10.0f, -0.5f, -35.0f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_box(rr, vp, (RtsVec3f){140.0f, -2.4f, -150.0f}, (RtsVec3f){540.0f, 1.0f, 540.0f}, idq, rr->tex_ocean, (RtsVec4f){0.92f, 0.98f, 1.0f, 1.0f}, light, time_s);

        /* Towering 1:1 Syd-Mead Octagonal Solaris Citadel + Analytic Hexagonal Energy Shield Dome */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_citadel, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){12.0f, 0.0f, -34.0f}, (RtsVec3f){14.5f, 14.5f, 14.5f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){12.0f, 2.0f, -34.0f}, (RtsVec3f){38.0f, 32.0f, 38.0f}),
                  (RtsVec4f){0.20f, 0.92f, 1.0f, 0.56f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        /* Sleek Sculpted VTOL Dropship Parked on Tarmac */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.62), (RtsVec3f){-14.5f, 0.2f, 9.0f}, (RtsVec3f){3.8f, 3.8f, 3.8f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sleek Stealth Grav-Plasma Tanks Engaging Obsidian Crimson Tank */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.42), (RtsVec3f){-1.5f, 0.1f, 16.5f}, (RtsVec3f){2.5f, 2.5f, 2.5f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -2.35), (RtsVec3f){46.0f, 0.1f, 8.0f}, (RtsVec3f){2.6f, 2.6f, 2.6f}),
                  (RtsVec4f){1.0f, 0.96f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Optional Voxel Fortification Bunker Module */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_voxel_sample, rr->tex_deck_floor, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){-33.0f, 0.0f, -6.0f}, (RtsVec3f){1.25f, 1.25f, 1.25f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Needle-Nosed Delta Starfighters Streaking Across the Sunset Ring Sky */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.65), rts_quat_from_axis_angle(rts_v3d(0,0,1), -0.35)), (RtsVec3f){-16.0f, 27.0f, -20.0f}, (RtsVec3f){2.6f, 2.6f, 2.6f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0,1,0), -2.1), rts_quat_from_axis_angle(rts_v3d(0,0,1), 0.32)), (RtsVec3f){28.0f, 33.0f, -40.0f}, (RtsVec3f){2.8f, 2.8f, 2.8f}),
                  (RtsVec4f){1.0f, 0.96f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Hybrid Crew Visuals: Sculpted 3D Marine + Crisp 32x64 8-Dir Sprites */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.45), (RtsVec3f){-18.0f, 0.0f, 31.5f}, (RtsVec3f){1.45f, 1.45f, 1.45f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_billboard_sprite(rr, vp, (RtsVec3f){-8.0f, 0.0f, 22.0f}, cam, 2.3f, 3.4f, 1, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);
        draw_billboard_sprite(rr, vp, (RtsVec3f){-4.5f, 0.0f, 24.5f}, cam, 2.3f, 3.4f, 3, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);
        draw_billboard_sprite(rr, vp, (RtsVec3f){ 4.5f, 0.0f, 21.0f}, cam, 2.3f, 3.4f, 6, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);

        /* Radiant Coral-Crimson vs Electric Cyan Plasma Beams */
        draw_plasma_beam(rr, vp, (RtsVec3f){43.0f, 2.8f, 7.2f}, (RtsVec3f){22.0f, 12.0f, -14.0f}, 0.42f, (RtsVec4f){1.0f, 0.24f, 0.42f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-0.5f, 2.9f, 15.0f}, (RtsVec3f){32.0f, 4.2f, 9.0f}, 0.36f, (RtsVec4f){0.22f, 0.96f, 1.0f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-14.0f, 27.0f, -20.0f}, (RtsVec3f){18.0f, 31.0f, -35.0f}, 0.32f, (RtsVec4f){0.28f, 1.0f, 0.82f, 1.0f}, time_s);

        bool has_mod = rr->vfs && rr->vfs->mod_count > 0;
        render_hud_common(rr, world, input,
                          has_mod ? "HELIOS PRIME SURFACE - MOD ACTIVE: EMERALD VANGUARD" : "HELIOS PRIME SURFACE - SOLARIS CITADEL [1:1 SCALE]",
                          has_mod ? "VFS MOD OVERLAY: MODS/EXAMPLE_MOD | JADE-EMERALD ARMOR + LUA UNITS" : "TWILIGHT RING SKY | HEX SHIELD:94% | SCULPTED 3D + 8-DIR SPRITES",
                          has_mod ? "MOD SDK: LIVE EXTERNAL EMERALD VANGUARD HULL & LUA UNIT OVERRIDE ACTIVE!" : "GUNNER: GRAV-PLASMA TANKS ENGAGING CRIMSON VANGUARD! DROPSHIP VALKYRIE-2 READY.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_DROPSHIP_INTERIOR) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.02f, 0.03f, 0.07f, 1.0f});
        RtsVec3f cam = {0.0f, 2.0f, 6.2f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.05f, 0.14f, 0.24f, 0.18f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 1.95f, -14.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_sky_dome(rr, vp, cam, sun, 1.0f, time_s);

        /* Lush Illuminated Helios Prime Curvature (tilted to equatorial oceans & continents) + Escort Starfighter & Dreadnought */
        RtsQuatd planet_rot = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), 1.55), rts_quat_from_axis_angle(rts_v3d(0,1,0), 2.15));
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(planet_rot, (RtsVec3f){14.0f, -68.0f, -135.0f}, (RtsVec3f){76.0f, 76.0f, 76.0f}),
                  (RtsVec4f){0.84f, 0.92f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.45), rts_quat_from_axis_angle(rts_v3d(0,0,1), 0.25)), (RtsVec3f){-3.8f, 4.2f, -24.0f}, (RtsVec3f){1.8f, 1.8f, 1.8f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){15.0f, 9.5f, -66.0f}, (RtsVec3f){2.8f, 2.8f, 2.8f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Vaulted Ribbed Dropship Fuselage Troop Bay */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.2f, 0.0f}, (RtsVec3f){8.2f, 0.4f, 16.5f}, idq, rr->tex_deck_floor, (RtsVec4f){0.94f, 0.96f, 1.0f, 1.0f}, light, time_s);
        for (int i = 0; i < 5; ++i) {
            float rz = 4.6f - (float)i * 3.1f;
            draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_blue, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.0f, rz}, (RtsVec3f){0.42f, 0.46f, 0.85f}),
                      (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
        /* Sculpted 3D Escort Marines Inside Troop Bay */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.35), (RtsVec3f){-2.3f, 0.0f, 1.2f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -1.35), (RtsVec3f){ 2.3f, 0.0f, -1.6f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Subtle Hexagonal Atmosphere Containment Field Across Open Bay */
        draw_mesh(rr, rr->sh_shield, rr->mesh_cube, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 2.0f, -7.8f}, (RtsVec3f){7.2f, 4.0f, 0.06f}),
                  (RtsVec4f){0.20f, 0.92f, 1.0f, 0.16f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        render_hud_common(rr, world, input,
                          "DROPSHIP 'VALKYRIE-2' BAY - ORBITAL ASCENT",
                          "ALT:84.2KM | VEL:4,820 M/S | 1.0G ARTIFICIAL | ESCAPE POD ARMED",
                          "PILOT: CLEARING UPPER ATMOSPHERE! MAG-SHIELDS AT 98%. APPROACHING SHIPYARD ALPHA.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_NAVAL_CARRIER_DECK) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.06f, 0.20f, 0.36f, 1.0f});
        RtsVec3f cam = {-5.5f, 3.8f, 28.0f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.42f, 0.44f, 0.54f, 0.72f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){6.5f, 6.2f, -32.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_sky_dome(rr, vp, cam, sun, 0.0f, time_s);

        /* Shimmering Stylized Sapphire-Teal Ocean Horizon */
        draw_box(rr, vp, (RtsVec3f){0.0f, -6.5f, -40.0f}, (RtsVec3f){620.0f, 2.0f, 620.0f}, idq, rr->tex_ocean, (RtsVec4f){0.94f, 0.98f, 1.0f, 1.0f}, light, time_s);

        /* 1:1 Sculpted Trimaran Carrier Deck + Sleek Stealth ATC Island Silhouette */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.4f, -18.0f}, (RtsVec3f){42.0f, 0.8f, 145.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.94f, 0.96f, 1.0f, 1.0f}, light, time_s);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_citadel, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.3), (RtsVec3f){21.5f, 0.0f, -15.0f}, (RtsVec3f){5.4f, 8.2f, 7.6f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sleek Delta-Wing Starfighters Staged & Launching off the Electromagnetic Catapult */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.14), (RtsVec3f){-8.5f, 1.2f, 4.0f}, (RtsVec3f){3.2f, 3.2f, 3.2f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.14), rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.18)), (RtsVec3f){-4.5f, 9.8f, -36.0f}, (RtsVec3f){3.5f, 3.5f, 3.5f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){4.0f, 0.5f, -12.0f}, (RtsVec3f){3.4f, 3.4f, 3.4f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sister Trimaran Fleet Carrier & Obsidian Crimson Raider on Sunset Horizon */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_carrier, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.32), (RtsVec3f){-82.0f, -5.8f, -88.0f}, (RtsVec3f){8.0f, 8.0f, 8.0f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.45), (RtsVec3f){64.0f, -2.5f, -108.0f}, (RtsVec3f){6.8f, 6.8f, 6.8f}),
                  (RtsVec4f){1.0f, 0.96f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sculpted Deck Crew Directing Launch (3D up close + 32x64 8-dir sprite down deck) */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.8), (RtsVec3f){-1.5f, 0.0f, 14.5f}, (RtsVec3f){1.30f, 1.30f, 1.30f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_billboard_sprite(rr, vp, (RtsVec3f){6.5f, 0.0f, 8.0f}, cam, 2.2f, 3.1f, 2, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);

        /* Anti-Ship Plasma Volley & Ion Afterburner Ribbon */
        draw_plasma_beam(rr, vp, (RtsVec3f){52.0f, 2.5f, -96.0f}, (RtsVec3f){10.0f, 12.0f, -18.0f}, 0.60f, (RtsVec4f){1.0f, 0.24f, 0.42f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-4.5f, 9.8f, -36.0f}, (RtsVec3f){54.0f, 1.5f, -102.0f}, 0.50f, (RtsVec4f){0.22f, 0.96f, 1.0f, 1.0f}, time_s);

        render_hud_common(rr, world, input,
                          "FLEET CARRIER 'VANGUARD' - 340M FLIGHT DECK",
                          "OCEAN SECTOR 4 | COMMS RELAY: LINKED | SCRAMBLING STARFIGHTER WING",
                          "ATC TOWER: INTERCEPTOR FLIGHT ALPHA AWAY! CRIMSON DESTROYER DETECTED ON HORIZON!",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_FOUNDRY_INTERIOR) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.025f, 0.04f, 0.09f, 1.0f});
        RtsVec3f cam = {-2.5f, 2.6f, 14.5f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.06f, 0.14f, 0.24f, 0.35f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){2.0f, 3.4f, -12.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_sky_dome(rr, vp, cam, sun, 0.0f, time_s);

        /* Cavernous Vaulted Cathedral Assembly Hall */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.3f, 0.0f}, (RtsVec3f){38.0f, 0.6f, 48.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s);
        for (int i = 0; i < 5; ++i) {
            float rz = 12.0f - (float)i * 7.8f;
            draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_blue, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.0f, rz}, (RtsVec3f){1.75f, 1.55f, 1.45f}),
                      (RtsVec4f){0.82f, 0.90f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }

        /* Sleek Sculpted 1:1 Grav-Plasma Stealth Tank Under Active Cyan Plasma Assembly Welding */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.52), (RtsVec3f){2.5f, 0.2f, -4.5f}, (RtsVec3f){3.7f, 3.7f, 3.7f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_plasma_beam(rr, vp, (RtsVec3f){0.0f, 13.8f, -3.6f}, (RtsVec3f){2.4f, 4.3f, -4.3f}, 0.24f, (RtsVec4f){0.28f, 0.96f, 1.0f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){2.4f, 4.3f, -4.3f}, (RtsVec3f){4.6f, 5.8f, -3.2f}, 0.12f, (RtsVec4f){1.0f, 0.84f, 0.34f, 1.0f}, time_s);

        /* Foreman Angled CRT Terminal & Sculpted 3D Engineer Crew */
        draw_box(rr, vp, (RtsVec3f){6.8f, 1.25f, 6.2f}, (RtsVec3f){2.8f, 2.0f, 0.45f}, rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.45), rr->tex_crt_console, (RtsVec4f){1,1,1,1}, light, time_s);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.8), (RtsVec3f){5.0f, 0.0f, 4.8f}, (RtsVec3f){1.20f, 1.20f, 1.20f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.7), (RtsVec3f){-4.2f, 0.0f, 1.8f}, (RtsVec3f){1.20f, 1.20f, 1.20f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        float w = (float)rr->width, h = (float)rr->height;
        RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
        hud_border_box(rr, ortho, 26.0f, h - 294.0f, 360.0f, 226.0f,
                       (RtsVec4f){0.03f, 0.08f, 0.16f, 0.86f}, (RtsVec4f){0.25f, 0.96f, 0.84f, 0.92f});
        hud_text(rr, ortho, 40.0f, h - 278.0f, 1.32f, (RtsVec4f){0.35f, 1.0f, 0.86f, 1.0f}, "TACTICAL WRIST-PAD [COMMS: OK]");
        hud_text(rr, ortho, 40.0f, h - 248.0f, 1.12f, (RtsVec4f){0.98f, 0.82f, 0.34f, 1.0f}, "PRODUCTION QUEUE:");
        hud_text(rr, ortho, 40.0f, h - 224.0f, 1.08f, (RtsVec4f){0.92f, 0.97f, 1.0f, 1.0f}, "1. GRAV-PLASMA TANK   [78%]");
        hud_text(rr, ortho, 40.0f, h - 202.0f, 1.08f, (RtsVec4f){0.74f, 0.88f, 0.98f, 1.0f}, "2. ARMORED APC        [QUEUED]");
        hud_text(rr, ortho, 40.0f, h - 180.0f, 1.08f, (RtsVec4f){0.74f, 0.88f, 0.98f, 1.0f}, "3. MOBILE LANDSHIP    [QUEUED]");
        hud_text(rr, ortho, 40.0f, h - 146.0f, 1.12f, (RtsVec4f){0.35f, 0.94f, 1.0f, 1.0f}, "SENSORS: RECON SAT LEO-1 PASS");
        hud_text(rr, ortho, 40.0f, h - 122.0f, 1.08f, (RtsVec4f){1.0f, 0.45f, 0.54f, 1.0f}, "ACOUSTICS: INTERIOR MUFFLE 65%");
        hud_quad(rr, ortho, 40.0f, h - 94.0f, 320.0f, 12.0f, (RtsVec4f){0.08f, 0.22f, 0.32f, 1.0f});
        hud_quad(rr, ortho, 40.0f, h - 94.0f, 250.0f, 12.0f, (RtsVec4f){0.25f, 0.96f, 0.84f, 1.0f});

        render_hud_common(rr, world, input,
                          "VEHICLE FOUNDRY - 1:1 VAULTED ASSEMBLY HALL",
                          "QUEUE: GRAV-PLASMA TANK 78% | POWER: +400MW | WRIST-PAD OPEN",
                          "FOREMAN: TANK HULL WELDING COMPLETE IN 4 SECONDS. READY TO ROLL ONTO APRON.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.08f, 0.015f, 0.03f, 1.0f});
        RtsVec3f cam = {0.0f, 2.35f, 8.8f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.42f, 0.08f, 0.14f, 0.35f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.4f, 2.0f, -14.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));
        RtsVec3f red_light = {0.35f, 0.90f, 0.20f};

        draw_sky_dome(rr, vp, cam, sun, 2.0f, time_s);

        /* Vaulted Cathedral Bridge Arches Bathed in Dramatic Crimson-Coral Red Alert */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.3f, -2.0f}, (RtsVec3f){24.0f, 0.6f, 28.0f}, idq, rr->tex_deck_floor, (RtsVec4f){1.0f, 0.56f, 0.60f, 1.0f}, red_light, time_s);
        for (int i = 0; i < 4; ++i) {
            float az = 4.0f - (float)i * 5.2f;
            draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_red, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.0f, az}, (RtsVec3f){1.12f, 0.88f, 1.0f}),
                      (RtsVec4f){1.0f, 0.24f, 0.32f, 1.0f}, red_light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }

        /* Sculpted Obsidian & Molten-Coral Crimson Dreadnought Outside Panoramic Bridge Viewport */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.35), (RtsVec3f){2.5f, 6.0f, -54.0f}, (RtsVec3f){5.2f, 5.2f, 5.2f}),
                  (RtsVec4f){1.0f, 0.92f, 0.95f, 1.0f}, red_light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_plasma_beam(rr, vp, (RtsVec3f){18.0f, 7.5f, -44.0f}, (RtsVec3f){-18.0f, 5.0f, -24.0f}, 0.70f, (RtsVec4f){1.0f, 0.22f, 0.42f, 1.0f}, time_s);

        /* Sculpted Red-Alert Tactical Bridge Holo-Globe & Pedestal */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){-0.6f, 0.15f, -1.8f}, (RtsVec3f){2.0f, 0.72f, 2.0f}),
                  (RtsVec4f){0.85f, 0.45f, 0.55f, 1.0f}, red_light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        RtsQuatd spin = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 0.6);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(spin, (RtsVec3f){-0.6f, 2.25f, -1.8f}, (RtsVec3f){1.05f, 1.05f, 1.05f}),
                  (RtsVec4f){1.0f, 0.40f, 0.42f, 0.90f}, red_light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.4), (RtsVec3f){-0.6f, 2.25f, -1.8f}, (RtsVec3f){1.62f, 1.62f, 1.62f}),
                  (RtsVec4f){1.0f, 0.26f, 0.40f, 0.98f}, red_light, time_s, false, true, false, false, RTS_DRAW_LINES);

        /* Sculpted Solar Marines vs Obsidian-Coral Crimson Breachers Trading Plasma Bolts Inside CIC! */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.95), (RtsVec3f){-4.4f, 0.0f, 1.2f}, (RtsVec3f){1.20f, 1.20f, 1.20f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.72), (RtsVec3f){-3.0f, 0.0f, -4.2f}, (RtsVec3f){1.20f, 1.20f, 1.20f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -1.15), (RtsVec3f){5.6f, 0.0f, -1.6f}, (RtsVec3f){1.24f, 1.24f, 1.24f}),
                  (RtsVec4f){1.0f, 0.92f, 0.95f, 1.0f}, red_light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -1.35), (RtsVec3f){7.2f, 0.0f, -4.6f}, (RtsVec3f){1.24f, 1.24f, 1.24f}),
                  (RtsVec4f){1.0f, 0.92f, 0.95f, 1.0f}, red_light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        draw_plasma_beam(rr, vp, (RtsVec3f){-3.8f, 1.50f, 1.0f},  (RtsVec3f){5.2f, 1.58f, -1.6f}, 0.13f, (RtsVec4f){0.22f, 0.96f, 1.0f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){ 5.2f, 1.55f, -1.8f}, (RtsVec3f){-2.6f, 1.65f, -3.9f}, 0.13f, (RtsVec4f){1.0f, 0.25f, 0.42f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){ 6.8f, 1.52f, -4.5f}, (RtsVec3f){-3.6f, 1.32f, 0.8f}, 0.11f, (RtsVec4f){1.0f, 0.65f, 0.24f, 1.0f}, time_s);

        float w = (float)rr->width, h = (float)rr->height;
        RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
        hud_border_box(rr, ortho, w - 392.0f, 88.0f, 372.0f, 192.0f,
                       (RtsVec4f){0.14f, 0.02f, 0.05f, 0.86f}, (RtsVec4f){1.0f, 0.26f, 0.42f, 0.98f});
        hud_text(rr, ortho, w - 376.0f, 104.0f, 1.38f, (RtsVec4f){1.0f, 0.34f, 0.44f, 1.0f}, "!! REACTOR MELTDOWN: 00:08.4 !!");
        hud_text(rr, ortho, w - 376.0f, 134.0f, 1.12f, (RtsVec4f){1.0f, 0.84f, 0.34f, 1.0f}, "BOARDING ALERT: DECK 3 BRIDGE/CIC");
        hud_text(rr, ortho, w - 376.0f, 158.0f, 1.08f, (RtsVec4f){0.94f, 0.96f, 1.0f, 1.0f}, "SOLAR MARINES: 14 | BREACHERS: 6");
        hud_text(rr, ortho, w - 376.0f, 182.0f, 1.08f, (RtsVec4f){0.35f, 0.96f, 1.0f, 1.0f}, "HULL DECOMPRESSED | MAG-BOOTS: ON");
        hud_text(rr, ortho, w - 376.0f, 208.0f, 1.12f, (RtsVec4f){0.35f, 1.0f, 0.72f, 1.0f}, "ESCAPE POD BAY: 14M PORT BULKHEAD");
        hud_quad(rr, ortho, w - 376.0f, 240.0f, 338.0f, 14.0f, (RtsVec4f){0.28f, 0.04f, 0.08f, 1.0f});
        hud_quad(rr, ortho, w - 376.0f, 240.0f, 265.0f, 14.0f, (RtsVec4f){1.0f, 0.26f, 0.42f, 1.0f});

        render_hud_common(rr, world, input,
                          "DREADNOUGHT 'SOLARIS' - BRIDGE/CIC [RED ALERT]",
                          "MULTI-DECK BOARDING ACTION | IRONMAN EVACUATION COUNTDOWN",
                          "CHIEF ENGINEER: CORE CONTAINMENT FAILING! SPRINT TO PORT ESCAPE PODS IMMEDIATELY!",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_WEBXR_STEREO_HOLOGLOBE) {
        RtsXrFrameState xr;
        memset(&xr, 0, sizeof(xr));
        xr.session_active = true;
        xr.eye_count = 2;
        xr.head_pos = rts_v3d(0.0, 2.30, 5.8);
        xr.head_rot = rts_quat_identity();
        xr.right_aim_ray.origin = rts_v3d(0.72, 1.55, 3.9);
        xr.right_aim_ray.dir = rts_v3d_norm(rts_v3d(-0.18, 0.16, -0.97));
        xr.right_trigger_down = true;
        rts_renderer_render_webxr_stereo(rr, world, input, &xr, time_s);
        return;
    }

    /* RTS_SCENE_ORBITAL_SHIPYARD: Sweeping Curved Ring-Arch Shipyard + Dagger-Prow Dreadnought + Cosmic Nebula */
    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.02f, 0.02f, 0.06f, 1.0f});
    RtsVec3f cam = {-76.0f, 36.0f, 100.0f};
    rr->cur_cam_pos = cam;
    rr->cur_fog_color = (RtsVec4f){0.05f, 0.12f, 0.22f, 0.20f};
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){6.0f, 8.0f, -24.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

    draw_sky_dome(rr, vp, cam, sun, 1.0f, time_s);

    /* Lush Illuminated Helios Prime Curved Limb Sweeping Below (tilted to equatorial oceans & continents + natural Fresnel rim) */
    RtsQuatd orb_planet_rot = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.98), rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.35));
    draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
              rts_mat4_from_quat_trs(orb_planet_rot, (RtsVec3f){-80.0f, -252.0f, -95.0f}, (RtsVec3f){235.0f, 235.0f, 235.0f}),
              (RtsVec4f){0.82f, 0.90f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

    /* Colossal Curved Orbital Ring-Arch Shipyard + Docked 680m Dagger-Prow Dreadnought 'Solaris' */
    RtsQuatd dock_rot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), -2.02);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_shipyard, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){30.0f, 12.0f, -32.0f}, (RtsVec3f){11.8f, 11.8f, 11.8f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){24.0f, 10.0f, -26.0f}, (RtsVec3f){8.6f, 8.6f, 8.6f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){24.0f, 10.0f, -26.0f}, (RtsVec3f){19.0f, 17.0f, 48.0f}),
              (RtsVec4f){0.22f, 0.94f, 1.0f, 0.48f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

    /* Obsidian Crimson Battlecruiser + Escort Delta Starfighters & VTOL Dropship */
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.72), (RtsVec3f){-56.0f, 22.0f, -54.0f}, (RtsVec3f){7.5f, 7.5f, 7.5f}),
              (RtsVec4f){1.0f, 0.96f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.6), rts_quat_from_axis_angle(rts_v3d(0,0,1), -0.35)), (RtsVec3f){-14.0f, 22.0f, 16.0f}, (RtsVec3f){3.2f, 3.2f, 3.2f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.8), (RtsVec3f){-32.0f, 8.0f, 44.0f}, (RtsVec3f){3.2f, 3.2f, 3.2f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

    /* Coral-Crimson vs Electric Cyan Orbital Beam Lances */
    draw_plasma_beam(rr, vp, (RtsVec3f){-34.0f, 22.0f, -38.0f}, (RtsVec3f){14.0f, 14.0f, -26.0f}, 1.05f, (RtsVec4f){1.0f, 0.24f, 0.42f, 1.0f}, time_s);
    draw_plasma_beam(rr, vp, (RtsVec3f){12.0f, 10.0f, -22.0f}, (RtsVec3f){-44.0f, 20.0f, -46.0f}, 0.88f, (RtsVec4f){0.22f, 0.96f, 1.0f, 1.0f}, time_s);
    draw_plasma_beam(rr, vp, (RtsVec3f){-14.0f, 22.0f, 14.0f}, (RtsVec3f){-48.0f, 22.0f, -44.0f}, 0.42f, (RtsVec4f){0.28f, 1.0f, 0.82f, 1.0f}, time_s);

    render_hud_common(rr, world, input,
                      "ORBITAL SHIPYARD ALPHA [400KM LEO] - RING DRYDOCK",
                      "VEL:7,672 M/S | ECC:0.002 | INCL:14.3 DEG | DREADNOUGHT 'SOLARIS'",
                      "ASTROGATOR: KEPLERIAN RENDEZVOUS COMPLETE. CRIMSON CRUISER OPENING BEAM LANCE FIRE!",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

void rts_renderer_render_webxr_stereo(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, const RtsXrFrameState *xr, float time_s) {
    if (!rr || !rr->initialized) return;
    int half_w = rr->width / 2;
    int full_h = rr->height;
    float eye_aspect = (float)(half_w > 0 ? half_w : 1) / (float)(full_h > 0 ? full_h : 1);
    RtsMat4f eye_proj = rts_mat4_perspective((float)(66.0 * RTS_DEG2RAD), eye_aspect, 0.15f, 1200.0f);
    RtsVec3f sun = {0.62f, 0.36f, -0.70f};
    RtsVec3f light = {0.58f, 0.78f, 0.38f};
    RtsQuatd idq = rts_quat_identity();

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.02f, 0.03f, 0.08f, 1.0f});

    RtsVec3f base_head = (xr && xr->session_active) ? rts_v3d_to_f(xr->head_pos) : (RtsVec3f){0.0f, 2.30f, 5.8f};
    RtsVec3f hand_pos = (xr && xr->session_active) ? rts_v3d_to_f(xr->right_aim_ray.origin) : (RtsVec3f){0.72f, 1.55f, 3.9f};
    RtsVec3f hit_pt = {0.0f, 2.25f, 0.45f};
    const float ipd_half = 0.24f;

    for (int eye = 0; eye < 2; ++eye) {
        if (rr->gfx.set_viewport) {
            rr->gfx.set_viewport(rr->gfx.state, eye * half_w, 0, half_w, full_h);
        }
        float side = (eye == 0) ? -1.0f : 1.0f;
        RtsVec3f cam = {base_head.x + side * ipd_half, base_head.y, base_head.z};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.06f, 0.16f, 0.28f, 0.20f};
        RtsVec3f target = {side * 0.04f, 1.85f, -0.8f};
        RtsMat4f vp = rts_mat4_mul(eye_proj, rts_mat4_look_at(cam, target, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_sky_dome(rr, vp, cam, sun, 1.0f, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.25f, 0.0f}, (RtsVec3f){20.0f, 0.5f, 20.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s);
        for (int i = 0; i < 3; ++i) {
            float az = 2.0f - (float)i * 4.5f;
            draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_blue, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.0f, az}, (RtsVec3f){0.90f, 0.72f, 1.0f}),
                      (RtsVec4f){0.85f, 0.92f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 0.15f, -0.6f}, (RtsVec3f){2.05f, 0.76f, 2.05f}),
                  (RtsVec4f){0.52f, 0.66f, 0.86f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sculpted Full 3D Low-Poly Officers Forced in WebXR */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.75), (RtsVec3f){-2.9f, 0.0f, -1.6f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.85), (RtsVec3f){2.9f, 0.0f, -1.4f}, (RtsVec3f){1.08f, 1.08f, 1.08f}),
                  (RtsVec4f){1.0f, 0.98f, 0.94f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* 3D Planetary & Orbital Holo-Globe */
        RtsQuatd spin = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 0.45 + 0.5);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.25f, -0.6f}, (RtsVec3f){1.18f, 1.18f, 1.18f}),
                  (RtsVec4f){0.55f, 0.95f, 1.0f, 0.92f}, light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.25f, -0.6f}, (RtsVec3f){1.27f, 1.27f, 1.27f}),
                  (RtsVec4f){0.20f, 0.94f, 1.0f, 0.55f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.38), (RtsVec3f){0.0f, 2.25f, -0.6f}, (RtsVec3f){1.68f, 1.68f, 1.68f}),
                  (RtsVec4f){1.0f, 0.84f, 0.30f, 0.96f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1,0,0), -0.52), (RtsVec3f){0.0f, 2.25f, -0.6f}, (RtsVec3f){1.98f, 1.98f, 1.98f}),
                  (RtsVec4f){1.0f, 0.26f, 0.42f, 0.96f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

        /* Sleek VR Right Controller + Glowing Laser Aim-Ray Intersecting Holo-Globe + Gold Hit Reticle */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.20), hand_pos, (RtsVec3f){0.12f, 0.14f, 0.16f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_plasma_beam(rr, vp, hand_pos, hit_pt, 0.042f, (RtsVec4f){0.25f, 1.0f, 0.86f, 1.0f}, time_s);
        draw_mesh(rr, rr->sh_holo, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(spin, hit_pt, (RtsVec3f){0.11f, 0.11f, 0.11f}),
                  (RtsVec4f){1.0f, 0.86f, 0.28f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_TRIANGLES);

        /* Floating 3D Left-Hand Holo-Radar Wrist-Pad in VR Space */
        draw_box(rr, vp, (RtsVec3f){-0.85f, 1.48f, 4.10f}, (RtsVec3f){0.58f, 0.38f, 0.03f},
                 rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.45), rr->tex_crt_console, (RtsVec4f){0.55f, 1.0f, 0.92f, 1.0f}, light, time_s);
    }

    if (rr->gfx.set_viewport) {
        rr->gfx.set_viewport(rr->gfx.state, 0, 0, rr->width, rr->height);
    }
    float w = (float)rr->width, h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    hud_quad(rr, ortho, w * 0.5f - 1.5f, 64.0f, 3.0f, h - 116.0f, (RtsVec4f){0.22f, 0.92f, 1.0f, 0.88f});
    hud_border_box(rr, ortho, 16.0f, 78.0f, 246.0f, 26.0f, (RtsVec4f){0.03f, 0.08f, 0.16f, 0.82f}, (RtsVec4f){0.25f, 0.96f, 0.84f, 0.92f});
    hud_text(rr, ortho, 26.0f, 86.0f, 1.12f, (RtsVec4f){0.42f, 1.0f, 0.88f, 1.0f}, "LEFT EYE [WEBXR 6-DOF]");
    hud_border_box(rr, ortho, w * 0.5f + 16.0f, 78.0f, 258.0f, 26.0f, (RtsVec4f){0.03f, 0.08f, 0.16f, 0.82f}, (RtsVec4f){0.25f, 0.96f, 0.84f, 0.92f});
    hud_text(rr, ortho, w * 0.5f + 26.0f, 86.0f, 1.12f, (RtsVec4f){0.42f, 1.0f, 0.88f, 1.0f}, "RIGHT EYE [IPD 64MM]");

    render_hud_common(rr, world, input,
                      "WEBXR STEREOSCOPIC VR - SOLARIS WAR ROOM",
                      "DUAL-EYE STEREO | 3D CREW LOD FORCED | LASER AIM-RAY HIT",
                      "WEBXR SESSION ACTIVE: AIM-RAY LOCKED ON HELIOS PRIME 400KM ORBITAL FLEET!",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

void rts_renderer_render_live_world(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, float time_s) {
    if (!rr || !rr->initialized || !world) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    RtsMat4f proj = rts_mat4_perspective((float)(60.0 * RTS_DEG2RAD), aspect, 0.2f, 2000.0f);
    RtsVec3f sun = {0.62f, 0.36f, -0.70f};
    RtsVec3f light = {0.58f, 0.78f, 0.38f};

    RtsVec3d cam_world = rts_v3d(0.0, 0.0, 0.0);
    rts_interior_local_to_world(&world->interiors, world->commander.frame_id, world->commander.local_pos, &cam_world);
    RtsVec3f fwd = {
        (float)(sin(world->commander.yaw_rad) * cos(world->commander.pitch_rad)),
        (float)(sin(world->commander.pitch_rad)),
        (float)(-cos(world->commander.yaw_rad) * cos(world->commander.pitch_rad))
    };
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at((RtsVec3f){0, 0, 0}, fwd, (RtsVec3f){0, 1, 0}));
    rr->cur_cam_pos = (RtsVec3f){0, 0, 0};
    rr->cur_fog_color = (RtsVec4f){0.24f, 0.36f, 0.48f, 0.50f};

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.04f, 0.08f, 0.18f, 1.0f});
    draw_sky_dome(rr, vp, (RtsVec3f){0, 0, 0}, sun, 0.0f, time_s);

    for (size_t i = 0; i < world->entity_count; ++i) {
        const RtsEntity *e = &world->entities[i];
        if (!e->active) continue;
        RtsVec3f rel = rts_v3d_relative_f(e->pos, cam_world);
        if (rel.x * rel.x + rel.y * rel.y + rel.z * rel.z > 1800.0f * 1800.0f) continue;
        RtsGfxHandle mesh = rr->mesh_citadel;
        if (e->construct_class == RTS_CLASS_GROUND_VEHICLE) mesh = rr->mesh_tank;
        else if (e->construct_class == RTS_CLASS_NAVAL_SHIP) mesh = rr->mesh_carrier;
        else if (e->construct_class == RTS_CLASS_AIRCRAFT) mesh = rr->mesh_dropship;
        else if (e->construct_class == RTS_CLASS_SPACECRAFT) mesh = rr->mesh_dreadnought;
        else if (e->construct_class == RTS_CLASS_SPACE_STATION) mesh = rr->mesh_shipyard;
        RtsGfxHandle tex = (e->faction_id == 2) ? rr->tex_hull_red : rr->tex_hull_blue;
        RtsMat4f model = rts_mat4_from_quat_trs(e->rot, rel, (RtsVec3f){3.0f, 3.0f, 3.0f});
        draw_mesh(rr, rr->sh_mesh, mesh, tex, vp, model, (RtsVec4f){1,1,1,1}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    }

    render_hud_common(rr, world, input,
                      "1STPRTS LIVE FIRST-PERSON COMMANDER VIEW",
                      "WASD: WALK/PILOT | TAB: WRIST-PAD | E: INTERACT | 1-9: SHOWCASE SCENES",
                      "LIVE SIMULATION ACTIVE - FLOATING-ORIGIN 64-BIT CAMERA RELATIVE TO COMMANDER.",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

void rts_renderer_render_tactical(RtsRetroRenderer *rr, const RtsTacticalSim *sim, const RtsWorld *world, const RtsInputRouter *input, float time_s) {
    if (!rr || !rr->initialized || !sim || !world) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    RtsMat4f proj = rts_mat4_perspective((float)(62.0 * RTS_DEG2RAD), aspect, 0.2f, 1600.0f);
    RtsVec3f sun = {0.62f, 0.36f, -0.70f};
    RtsVec3f light = {0.58f, 0.78f, 0.38f};
    RtsQuatd idq = rts_quat_identity();

    /* First-person camera with subtle trauma-squared recoil/impact shake (strictly <= 4% viewport height) */
    RtsVec3f cam = {
        sim->player_pos.x + sim->camera_shake_offset.x * 0.22f,
        sim->player_pos.y + 2.3f + sim->camera_shake_offset.y * 0.22f,
        sim->player_pos.z
    };
    RtsVec3f fwd = {
        sinf(sim->player_yaw) * cosf(sim->player_pitch),
        sinf(sim->player_pitch) - 0.06f,
        -cosf(sim->player_yaw) * cosf(sim->player_pitch)
    };
    RtsVec3f tgt = {cam.x + fwd.x * 20.0f, cam.y + fwd.y * 20.0f, cam.z + fwd.z * 20.0f};
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, tgt, (RtsVec3f){0.0f, 1.0f, 0.0f}));
    rr->cur_cam_pos = cam;
    rr->cur_fog_color = sim->game_over ? (RtsVec4f){0.34f, 0.08f, 0.14f, 0.78f} : (RtsVec4f){0.26f, 0.36f, 0.48f, 0.68f};

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.04f, 0.09f, 0.18f, 1.0f});
    draw_sky_dome(rr, vp, cam, sun, sim->game_over ? 2.0f : 0.0f, time_s);

    /* Solaris Citadel Defensive Apron Terrain + Towering Sculpted Citadel + Shield Dome */
    draw_mesh(rr, rr->sh_mesh, rr->mesh_terrain, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, -0.5f, -10.0f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
              (RtsVec4f){0.86f, 0.90f, 0.96f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_box(rr, vp, (RtsVec3f){0.0f, -0.25f, 0.0f}, (RtsVec3f){80.0f, 0.5f, 80.0f}, idq,
             rr->tex_deck_floor, (RtsVec4f){0.74f, 0.80f, 0.90f, 1.0f}, light, time_s);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_citadel, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){0.0f, 0.0f, -46.0f}, (RtsVec3f){12.0f, 12.0f, 12.0f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

    /* Resupply Caches (Glowing Solar Gold Beacons during Breather Troughs) */
    for (int i = 0; i < RTS_TAC_MAX_PICKUPS; ++i) {
        if (!sim->pickups[i].active) continue;
        RtsVec3f p = sim->pickups[i].pos;
        float bob = 0.35f * sinf(time_s * 4.5f + (float)i);
        draw_mesh(rr, rr->sh_holo, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){p.x, p.y + bob, p.z}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){1.0f, 0.88f, 0.26f, 0.92f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){p.x, 0.25f, p.z}, (RtsVec3f){2.2f, 2.2f, 2.2f}),
                  (RtsVec4f){0.28f, 1.0f, 0.84f, 0.95f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    }

    /* Active Crimson Syndicate Hostiles + Telegraph Charge Rings + EMP Primed Halos + Hex Shields */
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        const RtsTacEnemy *e = &sim->enemies[i];
        if (!e->active) continue;
        float sq = (e->squash_timer_s > 0.0f) ? (1.0f + 0.22f * (e->squash_timer_s / 0.12f)) : 1.0f;
        float sy_sc = 1.0f / sq;
        RtsGfxHandle mesh = (e->archetype == RTS_ENEMY_INTERCEPTOR) ? rr->mesh_starfighter
                          : ((e->archetype == RTS_ENEMY_SHIELD_CRUISER) ? rr->mesh_tank : rr->mesh_dreadnought);
        float base_sc = (e->archetype == RTS_ENEMY_INTERCEPTOR) ? 1.55f
                      : ((e->archetype == RTS_ENEMY_SHIELD_CRUISER) ? 1.95f : 1.35f);
        double yaw = atan2((double)(sim->player_pos.x - e->pos.x), (double)(sim->player_pos.z - e->pos.z));
        if (e->archetype != RTS_ENEMY_SHIELD_CRUISER) {
            yaw += RTS_PI; /* Orient dagger prows & starfighter noses directly toward the Commander */
        }
        RtsQuatd erot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), yaw);
        RtsVec4f tint = (e->hit_flash_s > 0.0f) ? (RtsVec4f){1.4f, 1.4f, 1.4f, 1.0f}
                      : ((e->emp_primed_timer_s > 0.0f) ? (RtsVec4f){0.45f, 1.15f, 1.35f, 1.0f} : (RtsVec4f){1.0f, 0.92f, 0.96f, 1.0f});
        draw_mesh(rr, rr->sh_mesh, mesh, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(erot, e->pos, (RtsVec3f){base_sc * sq, base_sc * sy_sc, base_sc * sq}),
                  tint, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Shielded Cruiser Hex Shield */
        if (e->shield > 1.0f) {
            float sr = e->radius_m * 1.45f;
            draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                      rts_mat4_from_quat_trs(idq, e->pos, (RtsVec3f){sr, sr * 0.8f, sr}),
                      (RtsVec4f){1.0f, 0.34f, 0.48f, 0.50f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        }
        /* High-Contrast 420ms Attack Telegraph Ring (bright solar gold -> coral pulse before firing!) */
        if (e->telegraph_timer_s > 0.0f && e->telegraph_total_s > 0.0f) {
            float frac = 1.0f - rts_clampf(e->telegraph_timer_s / e->telegraph_total_s, 0.0f, 1.0f);
            float tr = e->radius_m * (2.6f - 1.3f * frac);
            draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){e->pos.x, e->pos.y + 0.6f, e->pos.z}, (RtsVec3f){tr, tr, tr}),
                      (RtsVec4f){1.0f, 0.92f - 0.55f * frac, 0.18f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
            draw_plasma_beam(rr, vp, e->pos,
                             (RtsVec3f){e->pos.x + e->telegraph_aim_dir.x * 8.0f, e->pos.y + e->telegraph_aim_dir.y * 8.0f, e->pos.z + e->telegraph_aim_dir.z * 8.0f},
                             0.08f + 0.14f * frac, (RtsVec4f){1.0f, 0.85f, 0.22f, 0.90f}, time_s);
        }
    }

    /* High-Contrast Glowing Projectiles (Electric Cyan Player Bolts vs Vivid Molten-Coral Enemy Bolts) */
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        const RtsTacProjectile *pr = &sim->projectiles[i];
        if (!pr->active) continue;
        RtsVec4f col = pr->from_player ? (RtsVec4f){0.22f, 1.0f, 0.96f, 1.0f} : (RtsVec4f){1.0f, 0.28f, 0.42f, 1.0f};
        RtsVec3f tail = {pr->pos.x - pr->vel.x * 0.08f, pr->pos.y - pr->vel.y * 0.08f, pr->pos.z - pr->vel.z * 0.08f};
        draw_plasma_beam(rr, vp, tail, pr->pos, pr->radius_m * 0.78f, col, time_s);
    }

    /* Directional Juice Particle Bursts */
    for (int i = 0; i < RTS_TAC_MAX_PARTICLES; ++i) {
        const RtsTacParticle *pt = &sim->particles[i];
        if (!pt->active) continue;
        float alpha = rts_clampf(pt->life_s / fmaxf(0.01f, pt->max_life_s), 0.0f, 1.0f);
        float sz = pt->size_m * 0.72f;
        draw_mesh(rr, rr->sh_holo, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, pt->pos, (RtsVec3f){sz, sz, sz}),
                  (RtsVec4f){pt->color.x, pt->color.y, pt->color.z, alpha * 0.88f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
    }

    /* High-Contrast 2D Tactical HUD (Ability Cooldown Bar, Wave Banner, Popups, Reticle Hitmarker, Game Over Retry) */
    float w = (float)rr->width, h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    /* Damage impact crimson vignette border */
    if (sim->damage_vignette_s > 0.0f || sim->game_over) {
        float va = sim->game_over ? 0.34f : (sim->damage_vignette_s * 1.1f);
        hud_quad(rr, ortho, 0.0f, 0.0f, w, 10.0f, (RtsVec4f){1.0f, 0.18f, 0.32f, va});
        hud_quad(rr, ortho, 0.0f, h - 10.0f, w, 10.0f, (RtsVec4f){1.0f, 0.18f, 0.32f, va});
    }

    /* Center High-Contrast Reticle + Hitmarker Pop (hidden during Game Over modal so zero UI overlap) */
    if (!sim->game_over) {
        RtsVec4f ret_col = (sim->reticle_flash_s > 0.0f) ? (RtsVec4f){1.0f, 0.90f, 0.25f, 1.0f} : (RtsVec4f){0.28f, 1.0f, 0.94f, 0.95f};
        float rsz = (sim->reticle_flash_s > 0.0f) ? 14.0f : 9.0f;
        hud_quad(rr, ortho, w * 0.5f - rsz, h * 0.5f - 1.5f, rsz * 2.0f, 3.0f, ret_col);
        hud_quad(rr, ortho, w * 0.5f - 1.5f, h * 0.5f - rsz, 3.0f, rsz * 2.0f, ret_col);
    }

    /* Wave Director Status Pill & Score/Energy Strip */
    char wave_str[96];
    snprintf(wave_str, sizeof(wave_str), "WAVE %d [%s] | SCORE: %.0f | KILLS: %u | GRAZES: %u",
             sim->wave_number,
             sim->wave_phase == RTS_WAVE_BREATHER ? "BREATHER - RESUPPLY" : "ASSAULT",
             sim->score, sim->enemies_killed, sim->near_misses_total);
    hud_border_box(rr, ortho, w * 0.5f - 280.0f, 68.0f, 560.0f, 28.0f,
                   (RtsVec4f){0.03f, 0.07f, 0.14f, 0.88f},
                   sim->wave_phase == RTS_WAVE_BREATHER ? (RtsVec4f){1.0f, 0.86f, 0.28f, 0.96f} : (RtsVec4f){0.26f, 0.96f, 1.0f, 0.95f});
    hud_text(rr, ortho, w * 0.5f - 266.0f, 77.0f, 1.15f, (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, wave_str);

    /* Bottom-Left Health / Exo-Shield / Tactical Energy / Lance Heat Bars */
    hud_border_box(rr, ortho, 18.0f, h - 148.0f, 290.0f, 90.0f, (RtsVec4f){0.03f, 0.07f, 0.14f, 0.88f}, (RtsVec4f){0.25f, 0.94f, 1.0f, 0.90f});
    char hp_str[64];
    snprintf(hp_str, sizeof(hp_str), "HP: %.0f  SHD: %.0f  ENG: %.0f", sim->health, sim->exo_shield, sim->tactical_energy);
    hud_text(rr, ortho, 28.0f, h - 138.0f, 1.08f, (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, hp_str);
    hud_quad(rr, ortho, 28.0f, h - 114.0f, 268.0f, 10.0f, (RtsVec4f){0.14f, 0.08f, 0.12f, 1.0f});
    hud_quad(rr, ortho, 28.0f, h - 114.0f, 268.0f * rts_clampf(sim->health / sim->max_health, 0.0f, 1.0f), 10.0f, (RtsVec4f){0.30f, 1.0f, 0.68f, 1.0f});
    hud_quad(rr, ortho, 28.0f, h - 98.0f, 268.0f, 10.0f, (RtsVec4f){0.06f, 0.14f, 0.24f, 1.0f});
    hud_quad(rr, ortho, 28.0f, h - 98.0f, 268.0f * rts_clampf(sim->exo_shield / sim->max_exo_shield, 0.0f, 1.0f), 10.0f, (RtsVec4f){0.24f, 0.94f, 1.0f, 1.0f});
    hud_quad(rr, ortho, 28.0f, h - 82.0f, 268.0f, 10.0f, (RtsVec4f){0.16f, 0.12f, 0.06f, 1.0f});
    hud_quad(rr, ortho, 28.0f, h - 82.0f, 268.0f * rts_clampf(sim->tactical_energy / sim->max_tactical_energy, 0.0f, 1.0f), 10.0f, (RtsVec4f){1.0f, 0.84f, 0.28f, 1.0f});

    /* Bottom-Right 5 Tactical Ability Cards (LMB / Q / SHF / F / H) */
    static const char *act_labels[RTS_ACT_COUNT] = {"[J] LANCE", "[Q] EMP", "[S] DASH", "[F] FLEET", "[H] REPR"};
    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
        float bx = w - 538.0f + (float)a * 104.0f;
        float by = h - 114.0f;
        bool ready = (sim->cooldown_s[a] <= 0.0f) && !(a == RTS_ACT_FLEET_STRIKE && sim->tactical_energy < 22.0f);
        RtsVec4f bcol = ready ? (RtsVec4f){0.28f, 0.98f, 0.92f, 0.95f} : (RtsVec4f){0.42f, 0.48f, 0.58f, 0.72f};
        hud_border_box(rr, ortho, bx, by, 96.0f, 52.0f, (RtsVec4f){0.03f, 0.07f, 0.14f, 0.88f}, bcol);
        hud_text(rr, ortho, bx + 8.0f, by + 10.0f, 1.0f, ready ? (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f} : (RtsVec4f){0.62f, 0.68f, 0.76f, 1.0f}, act_labels[a]);
        char cd_txt[16];
        if (ready) snprintf(cd_txt, sizeof(cd_txt), "READY");
        else snprintf(cd_txt, sizeof(cd_txt), "%.1fS", sim->cooldown_s[a]);
        hud_text(rr, ortho, bx + 16.0f, by + 30.0f, 1.0f, ready ? (RtsVec4f){1.0f, 0.88f, 0.30f, 1.0f} : (RtsVec4f){1.0f, 0.42f, 0.52f, 1.0f}, cd_txt);
    }

    /* Active Multisensory UI Pop Callouts (capped at 3 max, hidden during Game Over modal) */
    if (!sim->game_over) {
        float py = 108.0f;
        int shown = 0;
        for (int i = 0; i < RTS_TAC_MAX_POPUPS && shown < 3; ++i) {
            if (!sim->popups[i].active) continue;
            hud_border_box(rr, ortho, w * 0.5f - 170.0f, py, 340.0f, 26.0f, (RtsVec4f){0.04f, 0.07f, 0.15f, 0.86f}, sim->popups[i].color);
            hud_text(rr, ortho, w * 0.5f - 154.0f, py + 8.0f, 1.15f, sim->popups[i].color, sim->popups[i].text);
            py += 32.0f;
            shown++;
        }
    }

    /* Crisp Instant-Retry Game Over Modal */
    if (sim->game_over) {
        hud_border_box(rr, ortho, w * 0.5f - 290.0f, h * 0.5f - 95.0f, 580.0f, 190.0f,
                       (RtsVec4f){0.10f, 0.02f, 0.05f, 0.94f}, (RtsVec4f){1.0f, 0.28f, 0.42f, 1.0f});
        hud_text(rr, ortho, w * 0.5f - 220.0f, h * 0.5f - 68.0f, 2.0f, (RtsVec4f){1.0f, 0.32f, 0.44f, 1.0f}, "COMMANDER DOWN [IRONMAN]");
        char stat_str[96];
        snprintf(stat_str, sizeof(stat_str), "SURVIVED: %.1fS | SCORE: %.0f | KILLS: %u | EMP CRITS: %u",
                 sim->sim_time_s, sim->score, sim->enemies_killed, sim->emp_crits_total);
        hud_text(rr, ortho, w * 0.5f - 258.0f, h * 0.5f - 10.0f, 1.15f, (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, stat_str);
        hud_border_box(rr, ortho, w * 0.5f - 210.0f, h * 0.5f + 32.0f, 420.0f, 38.0f,
                       (RtsVec4f){0.04f, 0.14f, 0.20f, 0.96f}, (RtsVec4f){0.28f, 1.0f, 0.86f, 1.0f});
        hud_text(rr, ortho, w * 0.5f - 186.0f, h * 0.5f + 45.0f, 1.25f, (RtsVec4f){0.34f, 1.0f, 0.88f, 1.0f}, "PRESS [R] OR [ENTER] FOR INSTANT RETRY");
    }

    render_hud_common(rr, world, input,
                      "SOLARIS CITADEL DEFENSE - LIVE FIRST-PERSON TACTICAL COMBAT",
                      "WASD: MOVE | J: PLASMA | Q: EMP | SHIFT: DASH | F: FLEET STRIKE | H: REPAIR | R: RETRY",
                      sim->game_over ? "IRONMAN DEFEAT - INSTANT IN-MEMORY RETRY READY IN 0.25S (PRESS R/ENTER)!"
                                     : "TACTICAL LINK ACTIVE: PRIME SHIELDED CRUISERS WITH [Q] EMP FOR x2.4 PLASMA CRIT!",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

bool rts_renderer_capture_png(RtsRetroRenderer *rr, const char *out_path) {
    if (!rr || !rr->initialized || !rr->gfx.capture_png) return false;
    return rr->gfx.capture_png(rr->gfx.state, out_path, rr->width, rr->height);
}
