/* SPDX-License-Identifier: MIT */
#include "rts/combat_sim.h"
#include "rts/crew.h"
#include "rts/field_gear.h"
#include "rts/gfx.h"
#include "rts/hologlobe_ui.h"
#include "rts/interior_gen.h"
#include "rts/planet_mesh.h"
#include "rts/tactical_sim.h"
#include "rts/ui_menus.h"
#include "rts/vehicle_physics.h"
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
    rr->mesh_cache_count = 0;
    rr->initialized = false;
}

const RtsMeshCacheEntry *rts_renderer_get_mesh_entry(RtsRetroRenderer *rr, const char *rel_path) {
    if (!rr || !rr->initialized || !rel_path || rel_path[0] == '\0') {
        return NULL;
    }
    for (size_t i = 0; i < rr->mesh_cache_count; ++i) {
        if (strcmp(rr->mesh_cache[i].rel_path, rel_path) == 0) {
            return &rr->mesh_cache[i];
        }
    }
    if (rr->mesh_cache_count >= RTS_MESH_CACHE_MAX) {
        return NULL;
    }
    char *txt = rts_vfs_read_text(rr->vfs, "assets", rel_path, NULL);
    if (!txt) {
        return NULL;
    }
    RtsObjMesh m;
    if (!rts_obj_load_memory(txt, (RtsVec3f){1.0f, 1.0f, 1.0f}, &m)) {
        rts_vfs_free(txt);
        return NULL;
    }
    rts_vfs_free(txt);

    RtsVec3f bmin = { 1e9f,  1e9f,  1e9f};
    RtsVec3f bmax = {-1e9f, -1e9f, -1e9f};
    for (size_t i = 0; i < m.vert_count; ++i) {
        if (m.verts[i].px < bmin.x) bmin.x = m.verts[i].px;
        if (m.verts[i].py < bmin.y) bmin.y = m.verts[i].py;
        if (m.verts[i].pz < bmin.z) bmin.z = m.verts[i].pz;
        if (m.verts[i].px > bmax.x) bmax.x = m.verts[i].px;
        if (m.verts[i].py > bmax.y) bmax.y = m.verts[i].py;
        if (m.verts[i].pz > bmax.z) bmax.z = m.verts[i].pz;
    }
    float dx = bmax.x - bmin.x;
    float dy = bmax.y - bmin.y;
    float dz = bmax.z - bmin.z;
    float rad = 0.5f * sqrtf(dx * dx + dy * dy + dz * dz);
    if (rad < 0.05f) rad = 1.0f;

    RtsGfxHandle h = rr->gfx.create_mesh(rr->gfx.state, m.verts, m.vert_count, false);
    rts_obj_free(&m);
    if (h == RTS_GFX_INVALID) {
        return NULL;
    }

    RtsMeshCacheEntry *e = &rr->mesh_cache[rr->mesh_cache_count++];
    memset(e, 0, sizeof(*e));
    snprintf(e->rel_path, sizeof(e->rel_path), "%s", rel_path);
    e->handle = h;
    e->bounds_min = bmin;
    e->bounds_max = bmax;
    e->bounds_radius = rad;
    return e;
}

RtsGfxHandle rts_renderer_get_mesh(RtsRetroRenderer *rr, const char *rel_path) {
    const RtsMeshCacheEntry *e = rts_renderer_get_mesh_entry(rr, rel_path);
    return e ? e->handle : RTS_GFX_INVALID;
}

void rts_renderer_set_live_status(RtsRetroRenderer *rr, const char *mode_tag, const char *prompt, const char *toast) {
    if (!rr) return;
    if (mode_tag) snprintf(rr->live_mode_tag, sizeof(rr->live_mode_tag), "%s", mode_tag);
    if (prompt) snprintf(rr->live_prompt, sizeof(rr->live_prompt), "%s", prompt);
    if (toast) snprintf(rr->live_toast, sizeof(rr->live_toast), "%s", toast);
}

void rts_renderer_set_active_interior(RtsRetroRenderer *rr, const RtsMegaInterior *mega, const RtsCrewDeckSim *crew) {
    if (!rr) return;
    rr->active_mega = mega;
    rr->active_crew = crew;
}

void rts_renderer_set_rts_toolkit(RtsRetroRenderer *rr, const RtsHoloGlobeState *holo,
                                  const RtsFieldGearState *gear, const RtsEconomySystem *eco) {
    if (!rr) return;
    rr->active_holo = holo;
    rr->active_gear = gear;
    rr->active_eco = eco;
}

void rts_renderer_set_live_combat_physics(RtsRetroRenderer *rr, const RtsCombatSim *combat,
                                          const RtsVehiclePhysicsState *vphys) {
    if (!rr) return;
    rr->active_combat = combat;
    rr->active_vphys = vphys;
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
    static RtsVertex verts[128 * 6];
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
    RtsVec3f lhand_pos = (xr && xr->session_active && rts_v3d_len_sq(xr->left_hand_pos) > 1e-4)
                       ? rts_v3d_to_f(xr->left_hand_pos) : (RtsVec3f){-0.85f, 1.48f, 4.10f};
    RtsVec3f hit_pt = {0.0f, 2.25f, 0.45f};
    if (xr && xr->session_active && rts_v3d_len_sq(xr->right_aim_ray.dir) > 0.5) {
        double t_hit = 0.0;
        if (rts_ray_intersect_sphere(xr->right_aim_ray, rts_v3d(0.0, 2.25, -0.6), 1.32, &t_hit)) {
            hit_pt = rts_v3d_to_f(rts_v3d_add(xr->right_aim_ray.origin, rts_v3d_scale(xr->right_aim_ray.dir, t_hit)));
        }
    }
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
        RtsMat4f vp = (xr && xr->has_eye_matrices)
                    ? rts_mat4_mul(xr->eyes[eye].proj, xr->eyes[eye].view)
                    : rts_mat4_mul(eye_proj, rts_mat4_look_at(cam, target, (RtsVec3f){0.0f, 1.0f, 0.0f}));

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

        /* Physical 6-DOF Left-Hand Diegetic Tactical Wrist-Pad Tracked to XR Controller */
        draw_box(rr, vp, lhand_pos, (RtsVec3f){0.62f, 0.40f, 0.03f},
                 rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.45), rr->tex_crt_console, (RtsVec4f){0.55f, 1.0f, 0.92f, 1.0f}, light, time_s);
    }

    if (rr->gfx.set_viewport) {
        rr->gfx.set_viewport(rr->gfx.state, 0, 0, rr->width, rr->height);
    }
    float w = (float)rr->width, h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    /* VR Comfort Vignette peripheral framing per eye when enabled */
    if (xr && xr->comfort_vignette) {
        RtsVec4f vig = {0.0f, 0.01f, 0.03f, 0.56f};
        hud_quad(rr, ortho, 0.0f, 64.0f, 28.0f, h - 116.0f, vig);
        hud_quad(rr, ortho, w * 0.5f - 28.0f, 64.0f, 56.0f, h - 116.0f, vig);
        hud_quad(rr, ortho, w - 28.0f, 64.0f, 28.0f, h - 116.0f, vig);
    }

    hud_quad(rr, ortho, w * 0.5f - 1.5f, 64.0f, 3.0f, h - 116.0f, (RtsVec4f){0.22f, 0.92f, 1.0f, 0.88f});
    hud_border_box(rr, ortho, 16.0f, 78.0f, 290.0f, 26.0f, (RtsVec4f){0.03f, 0.08f, 0.16f, 0.84f}, (RtsVec4f){0.25f, 0.96f, 0.84f, 0.92f});
    hud_text(rr, ortho, 26.0f, 86.0f, 1.05f, (RtsVec4f){0.42f, 1.0f, 0.88f, 1.0f},
             (xr && xr->has_eye_matrices) ? "LEFT EYE [6-DOF XRPOSE LIVE]" : "LEFT EYE [WEBXR 6-DOF]");
    hud_border_box(rr, ortho, w * 0.5f + 16.0f, 78.0f, 305.0f, 26.0f, (RtsVec4f){0.03f, 0.08f, 0.16f, 0.84f}, (RtsVec4f){0.25f, 0.96f, 0.84f, 0.92f});
    hud_text(rr, ortho, w * 0.5f + 26.0f, 86.0f, 1.05f, (RtsVec4f){0.42f, 1.0f, 0.88f, 1.0f},
             (xr && xr->comfort_vignette) ? "RIGHT EYE [VIGNETTE + SNAP 30]" : "RIGHT EYE [IPD 64MM]");

    render_hud_common(rr, world, input,
                      "WEBXR 6-DOF STEREOSCOPIC VR - SOLARIS WAR ROOM",
                      "TRUE XRVIEWERPOSE MATRICES | LEFT-HAND 3D WRIST-PAD | RIGHT LASER SELECTOR",
                      "6-DOF WEBXR LIVE: EYE MATRICES STREAMING + 3D WRIST-PAD + HOLO-GLOBE AIM RAY!",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

static void upper_trunc_copy(char *dst, size_t dst_sz, const char *src, size_t max_chars) {
    if (!dst || dst_sz == 0) return;
    size_t limit = dst_sz - 1;
    if (max_chars < limit) limit = max_chars;
    size_t i = 0;
    while (src && src[i] && i < limit) {
        char c = src[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        dst[i++] = c;
    }
    dst[i] = '\0';
}

void rts_renderer_render_roster_class(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, RtsConstructClass cls, float time_s) {
    (void)input;
    if (!rr || !rr->initialized || !world) return;

    const RtsConstructBlueprint *items[RTS_MAX_BLUEPRINTS];
    size_t count = 0;
    for (size_t i = 0; i < world->blueprint_count; ++i) {
        if (world->blueprints[i].active && world->blueprints[i].construct_class == cls) {
            items[count++] = &world->blueprints[i];
        }
    }

    int cols = 5, rows = 2;
    const char *cls_title = "CONSTRUCT ROSTER GALLERY";
    float sky_mode = 1.0f;
    bool hover_above = false;
    if (cls == RTS_CLASS_BUILDING) {
        cols = 6; rows = 4; sky_mode = 0.0f;
        cls_title = "CLASS 0: PLANETARY BUILDINGS [24 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    } else if (cls == RTS_CLASS_GROUND_VEHICLE) {
        cols = 5; rows = 2; sky_mode = 0.0f;
        cls_title = "CLASS 1: GROUND VEHICLES [10 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    } else if (cls == RTS_CLASS_NAVAL_SHIP) {
        cols = 5; rows = 2; sky_mode = 0.0f;
        cls_title = "CLASS 2: NAVAL SHIPS [10 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    } else if (cls == RTS_CLASS_AIRCRAFT) {
        cols = 4; rows = 2; sky_mode = 0.0f; hover_above = true;
        cls_title = "CLASS 3: AIRCRAFT & TRANSPORTS [8 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    } else if (cls == RTS_CLASS_SPACECRAFT) {
        cols = 5; rows = 3; sky_mode = 1.0f; hover_above = true;
        cls_title = "CLASS 4: ORBITAL & DEEP-SPACE CRAFT [14 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    } else if (cls == RTS_CLASS_SPACE_STATION) {
        cols = 5; rows = 2; sky_mode = 1.0f; hover_above = true;
        cls_title = "CLASS 5: SPACE STATIONS & DRYDOCKS [10 UNIQUE SCULPTED MODELS | ALL ENTERABLE]";
    }

    float w = (float)rr->width;
    float h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    RtsVec3f sun = {0.60f, 0.42f, -0.68f};
    RtsVec3f light = {0.58f, 0.84f, 0.48f};
    RtsQuatd idq = rts_quat_identity();

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.02f, 0.04f, 0.09f, 1.0f});
    RtsMat4f bg_proj = rts_mat4_perspective((float)(55.0 * RTS_DEG2RAD), w / (h > 0.0f ? h : 1.0f), 0.2f, 1200.0f);
    RtsMat4f bg_vp = rts_mat4_mul(bg_proj, rts_mat4_look_at((RtsVec3f){0, 0, 0}, (RtsVec3f){0, 0.12f, -1.0f}, (RtsVec3f){0, 1, 0}));
    rr->cur_cam_pos = (RtsVec3f){0, 0, 0};
    rr->cur_fog_color = (RtsVec4f){0.04f, 0.08f, 0.16f, 0.12f};
    draw_sky_dome(rr, bg_vp, (RtsVec3f){0, 0, 0}, sun, sky_mode, time_s);

    float pad_x = 16.0f;
    float pad_top = 66.0f;
    float pad_bot = 46.0f;
    float gap = 10.0f;
    float cell_w = (w - 2.0f * pad_x - (float)(cols - 1) * gap) / (float)cols;
    float cell_h = (h - pad_top - pad_bot - (float)(rows - 1) * gap) / (float)rows;
    float label_h = (rows >= 4) ? 38.0f : 44.0f;

    /* Draw frosted obsidian-slate tile backgrounds first so 3D viewports sit crisply on top */
    for (size_t i = 0; i < count; ++i) {
        int col = (int)(i % (size_t)cols);
        int row = (int)(i / (size_t)cols);
        float tx = pad_x + (float)col * (cell_w + gap);
        float ty = pad_top + (float)row * (cell_h + gap);
        hud_quad(rr, ortho, tx, ty, cell_w, cell_h, (RtsVec4f){0.03f, 0.06f, 0.13f, 0.72f});
    }

    bool face_prow_hero = (cls == RTS_CLASS_GROUND_VEHICLE || cls == RTS_CLASS_NAVAL_SHIP ||
                           cls == RTS_CLASS_AIRCRAFT || cls == RTS_CLASS_SPACECRAFT);

    /* Render each unique sculpted .obj construct inside its own distortion-free studio sub-viewport */
    for (size_t i = 0; i < count; ++i) {
        const RtsConstructBlueprint *bp = items[i];
        int col = (int)(i % (size_t)cols);
        int row = (int)(i / (size_t)cols);
        float tx = pad_x + (float)col * (cell_w + gap);
        float ty = pad_top + (float)row * (cell_h + gap);

        int vx = (int)(tx + 3.0f);
        int vw = (int)(cell_w - 6.0f);
        int vh = (int)(cell_h - label_h - 4.0f);
        int gl_vy = rr->height - (int)(ty + cell_h - label_h);
        if (vw < 8 || vh < 8) continue;

        if (rr->gfx.set_viewport) {
            rr->gfx.set_viewport(rr->gfx.state, vx, gl_vy, vw, vh);
        }
        float tile_aspect = (float)vw / (float)vh;
        RtsMat4f tproj = rts_mat4_perspective((float)(38.0 * RTS_DEG2RAD), tile_aspect, 0.15f, 80.0f);
        RtsVec3f cam = {0.0f, 2.15f, 5.05f};
        rr->cur_cam_pos = cam;
        rr->cur_fog_color = (RtsVec4f){0.04f, 0.08f, 0.16f, 0.08f};
        RtsMat4f vp = rts_mat4_mul(tproj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 0.05f, 0.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        /* Studio holographic turntable plinth & cyan calibration ring */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_deck_floor, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, -0.88f, 0.0f}, (RtsVec3f){1.78f, 0.07f, 1.78f}),
                  (RtsVec4f){0.26f, 0.38f, 0.56f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, -0.81f, 0.0f}, (RtsVec3f){1.86f, 1.86f, 1.86f}),
                  (RtsVec4f){0.24f, 0.94f, 1.0f, 0.90f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

        const RtsMeshCacheEntry *me = rts_renderer_get_mesh_entry(rr, bp->mesh_path);
        if (me && me->handle != RTS_GFX_INVALID) {
            float sc = 1.60f / me->bounds_radius;
            float cx = 0.5f * (me->bounds_min.x + me->bounds_max.x);
            float cz = 0.5f * (me->bounds_min.z + me->bounds_max.z);
            float base_y = hover_above ? -0.42f : -0.78f;
            float base_yaw = face_prow_hero ? 2.48f : 0.58f;
            float yaw = base_yaw + (float)(i % 3) * 0.09f + time_s * 0.08f;
            RtsQuatd q = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), yaw);
            RtsVec3d offset = rts_quat_rotate(q, rts_v3d(-cx * sc, 0.0, -cz * sc));
            RtsVec3f pos = {(float)offset.x, base_y - me->bounds_min.y * sc, (float)offset.z};
            RtsMat4f model = rts_mat4_from_quat_trs(q, pos, (RtsVec3f){sc, sc, sc});
            draw_mesh(rr, rr->sh_mesh, me->handle, rr->tex_hull_blue, vp, model,
                      (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
    }

    if (rr->gfx.set_viewport) {
        rr->gfx.set_viewport(rr->gfx.state, 0, 0, rr->width, rr->height);
    }

    /* Draw crisp tile borders and two-line razor-sharp Solaris metadata labels (>= 1.0x pixel font scale) */
    float name_scale = (rows >= 4) ? 1.0f : 1.12f;
    float meta_scale = 1.0f;
    size_t max_name_ch = (size_t)((cell_w - 14.0f) / (7.36f * name_scale));
    size_t max_meta_ch = (size_t)((cell_w - 14.0f) / (7.36f * meta_scale));
    if (max_name_ch < 8) max_name_ch = 8;
    if (max_meta_ch < 10) max_meta_ch = 10;

    for (size_t i = 0; i < count; ++i) {
        const RtsConstructBlueprint *bp = items[i];
        int col = (int)(i % (size_t)cols);
        int row = (int)(i / (size_t)cols);
        float tx = pad_x + (float)col * (cell_w + gap);
        float ty = pad_top + (float)row * (cell_h + gap);

        hud_quad(rr, ortho, tx, ty + cell_h - label_h, cell_w, label_h, (RtsVec4f){0.02f, 0.05f, 0.11f, 0.94f});
        hud_border_box(rr, ortho, tx, ty, cell_w, cell_h,
                       (RtsVec4f){0.0f, 0.0f, 0.0f, 0.0f}, (RtsVec4f){0.24f, 0.90f, 1.0f, 0.86f});

        char name_buf[64];
        upper_trunc_copy(name_buf, sizeof(name_buf), bp->name, max_name_ch);
        hud_text(rr, ortho, tx + 8.0f, ty + cell_h - label_h + 7.0f, name_scale,
                 (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, name_buf);

        char meta_raw[96];
        snprintf(meta_raw, sizeof(meta_raw), "%.0fM | %u RM%s%s%s",
                 bp->dimensions_m.x, bp->room_count,
                 bp->has_holo_globe ? " | HOLO" : "",
                 bp->has_escape_pod ? " | POD" : "",
                 bp->can_orbit ? " | ORB" : "");
        char meta_buf[64];
        upper_trunc_copy(meta_buf, sizeof(meta_buf), meta_raw, max_meta_ch);
        hud_text(rr, ortho, tx + 8.0f, ty + cell_h - label_h + (rows >= 4 ? 22.0f : 26.0f), meta_scale,
                 (RtsVec4f){0.34f, 0.96f, 0.92f, 1.0f}, meta_buf);
    }

    /* Top Header & Bottom Footer Banners */
    hud_border_box(rr, ortho, 16.0f, 10.0f, w - 32.0f, 46.0f,
                   (RtsVec4f){0.03f, 0.06f, 0.13f, 0.86f}, (RtsVec4f){0.24f, 0.92f, 1.0f, 0.92f});
    hud_text(rr, ortho, 28.0f, 18.0f, 1.35f, (RtsVec4f){0.94f, 0.98f, 1.0f, 1.0f}, cls_title);
    char sub[128];
    snprintf(sub, sizeof(sub), "SOLARIS RETRO-FUTURISM CATALOG | %zu / 76 UNIQUE .OBJ MESHES & MULTI-DECK INTERIORS | ZERO BAKED ASSETS", count);
    hud_text(rr, ortho, 28.0f, 38.0f, 1.0f, (RtsVec4f){0.96f, 0.80f, 0.36f, 1.0f}, sub);

    hud_border_box(rr, ortho, 16.0f, h - 38.0f, w - 32.0f, 28.0f,
                   (RtsVec4f){0.03f, 0.06f, 0.13f, 0.86f}, (RtsVec4f){0.96f, 0.78f, 0.34f, 0.86f});
    hud_text(rr, ortho, 28.0f, h - 29.0f, 1.05f, (RtsVec4f){0.86f, 0.96f, 1.0f, 1.0f},
             "EVERY CONSTRUCT LOADS ITS DEDICATED EXTERNAL ASSETS/MODELS/<ID>.OBJ VIA VFS AND HYDRATES MULTI-DECK INTERIORS FROM LUA");

    rr->gfx.end_frame(rr->gfx.state);
}

static RtsVec4f minimap_room_color(const RtsInteriorRoom *rm) {
    if (rm->damaged_pct > 1.0f || rm->breached) return (RtsVec4f){0.98f, 0.24f, 0.36f, 0.92f};
    switch (rm->kind) {
        case RTS_ROOM_WAR_ROOM:
        case RTS_ROOM_BRIDGE_CIC:  return (RtsVec4f){0.96f, 0.78f, 0.28f, 0.92f};
        case RTS_ROOM_TURBOLIFT:   return (RtsVec4f){1.0f, 0.62f, 0.22f, 0.92f};
        case RTS_ROOM_REACTOR:
        case RTS_ROOM_SHIELD_CORE: return (RtsVec4f){0.20f, 0.92f, 1.0f, 0.90f};
        case RTS_ROOM_HANGAR:      return (RtsVec4f){0.26f, 0.60f, 0.96f, 0.88f};
        case RTS_ROOM_MEDBAY:      return (RtsVec4f){0.36f, 0.98f, 0.78f, 0.88f};
        case RTS_ROOM_ESCAPE_POD:  return (RtsVec4f){0.22f, 0.96f, 0.56f, 0.92f};
        case RTS_ROOM_WEAPONS:
        case RTS_ROOM_ARMORY:
        case RTS_ROOM_BARRACKS:    return (RtsVec4f){0.56f, 0.44f, 0.88f, 0.85f};
        case RTS_ROOM_CORRIDOR:
        case RTS_ROOM_TRAM_STATION:return (RtsVec4f){0.24f, 0.36f, 0.52f, 0.88f};
        default:                   return (RtsVec4f){0.18f, 0.28f, 0.44f, 0.85f};
    }
}

static void draw_room_edge_wall(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world, RtsFrameId fid,
                                RtsQuatd frot, RtsVec3d cam_world, RtsVec3d mid_local, float span,
                                bool along_x, bool has_door, bool panoramic, RtsVec4f wall_tint, RtsVec3f light, float time_s) {
    float wall_h = 3.8f;
    float thick = 0.24f;
    if (has_door) {
        /* Wide 3.6m walk-through doorway flanked by sleek left/right bulkhead jambs & glowing transom */
        float door_w = fminf(3.6f, span * 0.48f);
        float side_w = fmaxf(0.4f, 0.5f * (span - door_w));
        float off = 0.5f * (door_w + side_w);
        for (int s = -1; s <= 1; s += 2) {
            RtsVec3d p = mid_local;
            if (along_x) p.x += (double)s * (double)off; else p.z += (double)s * (double)off;
            p.y += wall_h * 0.5;
            RtsVec3d pw;
            rts_interior_local_to_world(&world->interiors, fid, p, &pw);
            RtsVec3f sc = along_x ? (RtsVec3f){side_w, wall_h, thick} : (RtsVec3f){thick, wall_h, side_w};
            draw_box(rr, vp, rts_v3d_relative_f(pw, cam_world), sc, frot, rr->tex_hull_blue, wall_tint, light, time_s);
        }
        RtsVec3d tp = mid_local;
        tp.y += wall_h - 0.38;
        RtsVec3d tw;
        rts_interior_local_to_world(&world->interiors, fid, tp, &tw);
        RtsVec3f tsc = along_x ? (RtsVec3f){door_w + 0.2f, 0.72f, thick + 0.08f} : (RtsVec3f){thick + 0.08f, 0.72f, door_w + 0.2f};
        draw_box(rr, vp, rts_v3d_relative_f(tw, cam_world), tsc, frot, rr->tex_hull_blue, (RtsVec4f){0.36f, 0.94f, 1.0f, 1.0f}, light, time_s);
    } else if (panoramic) {
        /* Waist-high armored viewport sill so planetary horizon / orbital fleets remain visible outside */
        RtsVec3d p = mid_local;
        p.y += 0.48;
        RtsVec3d pw;
        rts_interior_local_to_world(&world->interiors, fid, p, &pw);
        RtsVec3f sc = along_x ? (RtsVec3f){span, 0.96f, thick} : (RtsVec3f){thick, 0.96f, span};
        draw_box(rr, vp, rts_v3d_relative_f(pw, cam_world), sc, frot, rr->tex_hull_blue, (RtsVec4f){0.62f, 0.76f, 0.92f, 1.0f}, light, time_s);
    } else {
        RtsVec3d p = mid_local;
        p.y += wall_h * 0.5;
        RtsVec3d pw;
        rts_interior_local_to_world(&world->interiors, fid, p, &pw);
        RtsVec3f sc = along_x ? (RtsVec3f){span, wall_h, thick} : (RtsVec3f){thick, wall_h, span};
        draw_box(rr, vp, rts_v3d_relative_f(pw, cam_world), sc, frot, rr->tex_hull_blue, wall_tint, light, time_s);
    }
}

static void draw_room_specific_props(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world,
                                     const RtsInteriorRoom *rm, RtsQuatd frot, RtsVec3d cam_world, RtsVec3f light, float time_s) {
    double cx = 0.5 * (rm->local_bounds.min.x + rm->local_bounds.max.x);
    double cz = 0.5 * (rm->local_bounds.min.z + rm->local_bounds.max.z);
    RtsVec3d center_w;
    rts_interior_local_to_world(&world->interiors, rm->frame_id, rts_v3d(cx, rm->local_bounds.min.y, cz), &center_w);
    RtsVec3f crel = rts_v3d_relative_f(center_w, cam_world);

    if (rm->kind == RTS_ROOM_REACTOR) {
        /* Towering Tokamak Fusion Reactor Core + Rotating Energy Containment Rings */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y + 0.35f, crel.z}, (RtsVec3f){1.9f, 0.45f, 1.9f}),
                  (RtsVec4f){0.42f, 0.54f, 0.72f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y + 2.0f, crel.z}, (RtsVec3f){0.78f, 1.65f, 0.78f}),
                  (RtsVec4f){0.22f, 0.96f, 1.0f, 0.92f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        RtsQuatd r1 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 1.1),
                                   rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.32));
        RtsQuatd r2 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), -time_s * 0.9),
                                   rts_quat_from_axis_angle(rts_v3d(0, 0, 1), 0.36));
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(r1, (RtsVec3f){crel.x, crel.y + 1.8f, crel.z}, (RtsVec3f){1.65f, 1.65f, 1.65f}),
                  (RtsVec4f){1.0f, 0.84f, 0.28f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(r2, (RtsVec3f){crel.x, crel.y + 2.3f, crel.z}, (RtsVec3f){1.85f, 1.85f, 1.85f}),
                  (RtsVec4f){0.25f, 0.96f, 1.0f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    } else if (rm->kind == RTS_ROOM_SHIELD_CORE) {
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y + 1.8f, crel.z}, (RtsVec3f){1.45f, 1.45f, 1.45f}),
                  (RtsVec4f){0.22f, 0.94f, 1.0f, 0.72f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
    } else if (rm->kind == RTS_ROOM_HANGAR) {
        /* Illuminated landing pad markings + proportionally scaled sculpted Solaris Starfighter */
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y + 0.08f, crel.z - 0.5f}, (RtsVec3f){2.85f, 2.85f, 2.85f}),
                  (RtsVec4f){1.0f, 0.84f, 0.30f, 0.92f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        RtsGfxHandle fighter = rts_renderer_get_mesh(rr, "models/air_starfighter.obj");
        if (fighter == RTS_GFX_INVALID) fighter = rr->mesh_starfighter;
        draw_mesh(rr, rr->sh_mesh, fighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 0.55), (RtsVec3f){crel.x - 0.4f, crel.y + 0.72f, crel.z - 0.6f}, (RtsVec3f){0.82f, 0.82f, 0.82f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    } else if (rm->kind == RTS_ROOM_TURBOLIFT) {
        /* Champagne-Gold & Cyan Vertical Turbolift Shaft Core */
        draw_box(rr, vp, (RtsVec3f){crel.x, crel.y + 1.9f, crel.z - 2.6f}, (RtsVec3f){2.2f, 3.8f, 1.2f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.96f, 0.80f, 0.36f, 1.0f}, light, time_s);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y + 0.12f, crel.z}, (RtsVec3f){1.75f, 1.75f, 1.75f}),
                  (RtsVec4f){1.0f, 0.84f, 0.32f, 0.95f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    } else if (rm->kind == RTS_ROOM_MEDBAY) {
        draw_box(rr, vp, (RtsVec3f){crel.x - 1.8f, crel.y + 0.65f, crel.z}, (RtsVec3f){1.1f, 1.1f, 2.2f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.76f, 1.0f, 0.94f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){crel.x + 1.8f, crel.y + 0.65f, crel.z}, (RtsVec3f){1.1f, 1.1f, 2.2f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.76f, 1.0f, 0.94f, 1.0f}, light, time_s);
    } else if (rm->kind == RTS_ROOM_ARMORY || rm->kind == RTS_ROOM_BARRACKS) {
        draw_box(rr, vp, (RtsVec3f){crel.x - 2.4f, crel.y + 1.1f, crel.z - 1.5f}, (RtsVec3f){0.9f, 2.2f, 2.4f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.42f, 0.92f, 0.74f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){crel.x + 2.4f, crel.y + 1.1f, crel.z - 1.5f}, (RtsVec3f){0.9f, 2.2f, 2.4f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.42f, 0.92f, 0.74f, 1.0f}, light, time_s);
    } else if (rm->kind == RTS_ROOM_CARGO || rm->kind == RTS_ROOM_ENGINEERING) {
        draw_box(rr, vp, (RtsVec3f){crel.x - 1.9f, crel.y + 0.85f, crel.z - 1.2f}, (RtsVec3f){1.6f, 1.6f, 1.8f}, frot,
                 rr->tex_hull_blue, (RtsVec4f){0.86f, 0.74f, 0.44f, 1.0f}, light, time_s);
    } else if (rm->kind == RTS_ROOM_CORRIDOR || rm->kind == RTS_ROOM_TRAM_STATION) {
        /* Signature Solaris Illuminated Deck Wayfinding Stripe */
        double rw = rm->local_bounds.max.x - rm->local_bounds.min.x;
        draw_box(rr, vp, (RtsVec3f){crel.x, crel.y + 0.03f, crel.z}, (RtsVec3f){(float)rw * 0.92f, 0.05f, 0.34f}, frot,
                 rr->tex_crt_console, (RtsVec4f){0.28f, 0.96f, 1.0f, 1.0f}, light, time_s);
    }
}

static void draw_deck_blueprint_minimap(RtsRetroRenderer *rr, RtsMat4f ortho, const RtsWorld *world, RtsFrameId fid) {
    float w = (float)rr->width;
    float mw = 272.0f;
    float mh = 194.0f;
    float mx = w - mw - 16.0f;
    float my = 72.0f;
    hud_border_box(rr, ortho, mx, my, mw, mh,
                   (RtsVec4f){0.02f, 0.05f, 0.12f, 0.90f},
                   (rr->active_crew && rr->active_crew->red_alert) ? (RtsVec4f){1.0f, 0.28f, 0.42f, 0.98f} : (RtsVec4f){0.24f, 0.92f, 1.0f, 0.94f});

    char hdr[64];
    if (rr->active_mega && rr->active_mega->total_rooms > 0u) {
        snprintf(hdr, sizeof(hdr), "DECK %u/%u | %u ROOMS",
                 (unsigned)(rr->active_mega->active_deck + 1u),
                 (unsigned)rr->active_mega->deck_count,
                 (unsigned)rr->active_mega->total_rooms);
    } else {
        snprintf(hdr, sizeof(hdr), "DECK BLUEPRINT MAP");
    }
    hud_text(rr, ortho, mx + 10.0f, my + 8.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.36f, 1.0f}, hdr);

    char sub[64];
    size_t active_c = (rr->active_crew) ? rr->active_crew->count : 0u;
    uint32_t total_c = (rr->active_mega) ? rr->active_mega->total_crew_capacity : 0u;
    snprintf(sub, sizeof(sub), "STAFF %zu LIVE / %u%s",
             active_c, (unsigned)total_c,
             (rr->active_crew && rr->active_crew->red_alert) ? " [ALERT]" : "");
    hud_text(rr, ortho, mx + 10.0f, my + 24.0f, 1.0f,
             (rr->active_crew && rr->active_crew->red_alert) ? (RtsVec4f){1.0f, 0.38f, 0.46f, 1.0f} : (RtsVec4f){0.36f, 0.98f, 0.90f, 1.0f}, sub);

    /* Compute bounding rectangle of all hydrated rooms on the active deck */
    double minx = 1e9, maxx = -1e9, minz = 1e9, maxz = -1e9;
    int rcnt = 0;
    for (size_t i = 0; i < world->interiors.room_count; ++i) {
        const RtsInteriorRoom *rm = &world->interiors.rooms[i];
        if (!rm->active || rm->frame_id != fid) continue;
        if (rm->local_bounds.min.x < minx) minx = rm->local_bounds.min.x;
        if (rm->local_bounds.max.x > maxx) maxx = rm->local_bounds.max.x;
        if (rm->local_bounds.min.z < minz) minz = rm->local_bounds.min.z;
        if (rm->local_bounds.max.z > maxz) maxz = rm->local_bounds.max.z;
        rcnt++;
    }
    if (rcnt == 0 || maxx <= minx || maxz <= minz) return;

    float pad_l = mx + 12.0f;
    float pad_t = my + 42.0f;
    float box_w = mw - 24.0f;
    float box_h = mh - 68.0f;
    float sc = fminf(box_w / (float)(maxx - minx), box_h / (float)(maxz - minz));
    float ox = pad_l + 0.5f * (box_w - (float)(maxx - minx) * sc);
    float oy = pad_t + 0.5f * (box_h - (float)(maxz - minz) * sc);

    for (size_t i = 0; i < world->interiors.room_count; ++i) {
        const RtsInteriorRoom *rm = &world->interiors.rooms[i];
        if (!rm->active || rm->frame_id != fid) continue;
        float rx = ox + (float)(rm->local_bounds.min.x - minx) * sc;
        float ry = oy + (float)(rm->local_bounds.min.z - minz) * sc;
        float rw = (float)(rm->local_bounds.max.x - rm->local_bounds.min.x) * sc;
        float rh = (float)(rm->local_bounds.max.z - rm->local_bounds.min.z) * sc;
        hud_quad(rr, ortho, rx + 1.5f, ry + 1.5f, fmaxf(2.0f, rw - 3.0f), fmaxf(2.0f, rh - 3.0f), minimap_room_color(rm));
        /* Bright cyan doorway ticks where rooms connect */
        if (rm->door_mask & RTS_DOOR_E) hud_quad(rr, ortho, rx + rw - 2.0f, ry + rh * 0.35f, 4.0f, fmaxf(2.0f, rh * 0.3f), (RtsVec4f){0.62f, 0.98f, 1.0f, 0.95f});
        if (rm->door_mask & RTS_DOOR_S) hud_quad(rr, ortho, rx + rw * 0.35f, ry + rh - 2.0f, fmaxf(2.0f, rw * 0.3f), 4.0f, (RtsVec4f){0.62f, 0.98f, 1.0f, 0.95f});
    }

    /* Live Staffed Crew Dots on Minimap */
    if (rr->active_crew) {
        for (size_t i = 0; i < rr->active_crew->count; ++i) {
            const RtsCrewAgent *a = &rr->active_crew->agents[i];
            if (!a->active) continue;
            float ax = ox + (float)(a->local_pos.x - minx) * sc;
            float ay = oy + (float)(a->local_pos.z - minz) * sc;
            RtsVec4f dot_c = (a->faction_id == 2u) ? (RtsVec4f){1.0f, 0.24f, 0.36f, 1.0f} : (RtsVec4f){0.78f, 1.0f, 0.96f, 0.95f};
            hud_quad(rr, ortho, ax - 1.8f, ay - 1.8f, 3.6f, 3.6f, dot_c);
        }
    }

    /* Commander Pulsing White-Gold Marker */
    float px = ox + (float)(world->commander.local_pos.x - minx) * sc;
    float py = oy + (float)(world->commander.local_pos.z - minz) * sc;
    hud_quad(rr, ortho, px - 3.5f, py - 3.5f, 7.0f, 7.0f, (RtsVec4f){1.0f, 0.96f, 0.38f, 1.0f});

    hud_text(rr, ortho, mx + 10.0f, my + mh - 18.0f, 1.0f, (RtsVec4f){0.82f, 0.92f, 1.0f, 1.0f}, "[/] LIFT | N SHIP | F ALERT");
}

static void draw_hologlobe_3d_projections(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world,
                                          RtsFrameId fid, RtsVec3d globe_center_local, RtsVec3d cam_world,
                                          RtsVec3f light, float time_s) {
    if (!rr->active_holo) return;
    const RtsHoloGlobeState *hg = rr->active_holo;
    RtsQuatd idq = rts_quat_identity();

    /* Outer Keplerian GEO Orbital Track Ring */
    RtsVec3d gc_w;
    rts_interior_local_to_world(&world->interiors, fid, globe_center_local, &gc_w);
    RtsVec3f gc_rel = rts_v3d_relative_f(gc_w, cam_world);
    draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.18 + hg->pitch_rad * 0.25), gc_rel, (RtsVec3f){1.56f, 1.56f, 1.56f}),
              (RtsVec4f){0.36f, 0.92f, 1.0f, 0.72f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

    /* Project Every Friendly Construct & Fog-of-War Detected Hostile Onto Spinning 3D Holo-Globe */
    for (size_t i = 0; i < world->entity_count; ++i) {
        const RtsEntity *e = &world->entities[i];
        if (!e->active) continue;
        if (e->faction_id != 1u && !rts_sensors_is_target_detected(world, 1u, e->id)) continue;

        RtsVec3d bp_local = rts_hologlobe_world_to_globe(hg, &world->planet, e->pos, globe_center_local, 0.92);
        RtsVec3d bp_w;
        rts_interior_local_to_world(&world->interiors, fid, bp_local, &bp_w);
        RtsVec3f bp_rel = rts_v3d_relative_f(bp_w, cam_world);

        bool sel = rts_hologlobe_is_selected(hg, e->id);
        bool hov = (hg->hovered_entity_id == e->id);
        RtsVec4f col = (e->faction_id == 2u) ? (RtsVec4f){1.0f, 0.28f, 0.44f, 0.98f}
                     : (sel ? (RtsVec4f){1.0f, 0.88f, 0.30f, 1.0f} : (RtsVec4f){0.24f, 0.96f, 1.0f, 0.96f});
        float bsz = (sel || hov) ? 0.065f : 0.046f;
        draw_mesh(rr, rr->sh_holo, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, bp_rel, (RtsVec3f){bsz, bsz, bsz}),
                  col, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        if (sel || hov) {
            RtsQuatd hq = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 1.8);
            draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                      rts_mat4_from_quat_trs(hq, bp_rel, (RtsVec3f){0.13f, 0.13f, 0.13f}),
                      (RtsVec4f){1.0f, 0.86f, 0.28f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        }

        /* Active Order Trajectory Arc on the 3D Holo-Globe */
        if (e->faction_id == 1u && (e->order_type == RTS_ORDER_MOVE || e->order_type == RTS_ORDER_ATTACK)) {
            RtsVec3d tp_local = rts_hologlobe_world_to_globe(hg, &world->planet, e->order_target_pos, globe_center_local, 0.92);
            RtsVec3d tp_w;
            rts_interior_local_to_world(&world->interiors, fid, tp_local, &tp_w);
            RtsVec4f acol = (e->order_type == RTS_ORDER_ATTACK) ? (RtsVec4f){1.0f, 0.36f, 0.42f, 0.95f} : (RtsVec4f){0.32f, 0.98f, 0.86f, 0.95f};
            draw_plasma_beam(rr, vp, bp_rel, rts_v3d_relative_f(tp_w, cam_world), 0.014f, acol, time_s);
        }
    }

    /* Live 3D Holographic Building / Station Placement Ghost on the Spinning Holo-Globe */
    if (hg->mode == RTS_HOLO_MODE_PLACE_BUILDING && hg->place_blueprint_id[0] != '\0') {
        RtsVec3d gp_local = rts_hologlobe_world_to_globe(hg, &world->planet, hg->ghost_world_pos, globe_center_local, 0.92);
        RtsVec3d gp_w;
        rts_interior_local_to_world(&world->interiors, fid, gp_local, &gp_w);
        RtsVec3f gp_rel = rts_v3d_relative_f(gp_w, cam_world);
        RtsVec4f gcol = hg->ghost_valid ? (RtsVec4f){0.28f, 1.0f, 0.72f, 0.92f} : (RtsVec4f){1.0f, 0.28f, 0.40f, 0.92f};

        const RtsConstructBlueprint *gbp = rts_world_find_blueprint(world, hg->place_blueprint_id);
        const RtsMeshCacheEntry *gme = gbp ? rts_renderer_get_mesh_entry(rr, gbp->mesh_path) : NULL;
        RtsGfxHandle gmesh = gme ? gme->handle : rr->mesh_citadel;
        float gsc = gme ? (0.14f / fmaxf(0.25f, gme->bounds_radius)) : 0.12f;
        draw_mesh(rr, rr->sh_holo, gmesh, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 1.2), gp_rel, (RtsVec3f){gsc, gsc, gsc}),
                  gcol, light, time_s, true, true, false, true, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, gp_rel, (RtsVec3f){0.19f, 0.19f, 0.19f}),
                  gcol, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    }
}

static void draw_live_gantry_assemblies(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world,
                                        RtsVec3d cam_world, RtsVec3f light, float time_s) {
    if (!rr->active_eco) return;
    RtsQuatd idq = rts_quat_identity();

    for (size_t i = 0; i < rr->active_eco->job_count; ++i) {
        const RtsProductionJob *job = &rr->active_eco->jobs[i];
        if (!job->active || job->completed) continue;
        const RtsEntity *prod = rts_world_get_entity_const(world, job->producer_entity_id);
        const RtsConstructBlueprint *bp = rts_world_find_blueprint(world, job->blueprint_id);
        if (!prod || !bp) continue;

        /* Position 1:1 Assembly Gantry visibly on the producer's sunlit fabrication apron */
        RtsVec3d gantry_w = rts_v3d(prod->pos.x + 28.0, prod->pos.y, prod->pos.z + 32.0);
        RtsVec3f grel = rts_v3d_relative_f(gantry_w, cam_world);
        if (grel.x * grel.x + grel.y * grel.y + grel.z * grel.z > 2400.0f * 2400.0f) continue;

        float prog = (job->total_time_s > 0.01)
            ? rts_clampf((float)(1.0 - job->remaining_s / job->total_time_s), 0.12f, 0.99f) : 0.5f;

        /* Illuminated Gantry Fabrication Platform & Calibration Ring */
        draw_box(rr, vp, (RtsVec3f){grel.x, grel.y - 0.18f, grel.z}, (RtsVec3f){14.0f, 0.36f, 14.0f}, idq,
                 rr->tex_deck_floor, (RtsVec4f){0.62f, 0.86f, 1.0f, 1.0f}, light, time_s);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 0.6), (RtsVec3f){grel.x, grel.y + 0.15f, grel.z}, (RtsVec3f){6.8f, 6.8f, 6.8f}),
                  (RtsVec4f){0.26f, 0.96f, 1.0f, 0.92f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

        /* 4 Corner Gantry Pylons Firing Golden Plasma Weld Arcs Onto Rising 1:1 Hull */
        const RtsMeshCacheEntry *me = rts_renderer_get_mesh_entry(rr, bp->mesh_path);
        RtsGfxHandle mesh = me ? me->handle : rr->mesh_tank;
        float sc = me ? rts_clampf((fmaxf(bp->dimensions_m.x, bp->dimensions_m.z) * 0.42f) / me->bounds_radius, 2.2f, 8.5f) : 3.4f;
        RtsQuatd mrot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 0.45);

        /* Full-Scale Cyan Wireframe Holographic Blueprint Scaffold */
        draw_mesh(rr, rr->sh_holo, mesh, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(mrot, (RtsVec3f){grel.x, grel.y + 1.2f, grel.z}, (RtsVec3f){sc * 1.04f, sc * 1.04f, sc * 1.04f}),
                  (RtsVec4f){0.22f, 0.96f, 1.0f, 0.88f}, light, time_s, true, true, false, true, RTS_DRAW_TRIANGLES);

        /* Rising Solid Sculpted .OBJ Hull Scaling Progressively With Build Progress */
        draw_mesh(rr, rr->sh_mesh, mesh, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(mrot, (RtsVec3f){grel.x, grel.y + 1.2f * prog, grel.z}, (RtsVec3f){sc, sc * prog, sc}),
                  (RtsVec4f){1.0f, 0.98f, 0.94f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        static const float p_off[4][2] = {{-5.6f, -5.6f}, {5.6f, -5.6f}, {-5.6f, 5.6f}, {5.6f, 5.6f}};
        for (int p = 0; p < 4; ++p) {
            RtsVec3f base_p = {grel.x + p_off[p][0], grel.y + 3.2f, grel.z + p_off[p][1]};
            draw_box(rr, vp, base_p, (RtsVec3f){0.65f, 6.4f, 0.65f}, idq, rr->tex_hull_blue, (RtsVec4f){0.82f, 0.88f, 0.98f, 1.0f}, light, time_s);
            RtsVec3f tip = {base_p.x, grel.y + 5.6f, base_p.z};
            RtsVec3f weld = {grel.x + sinf(time_s * 2.4f + (float)p) * 1.2f, grel.y + 1.0f + prog * 2.2f, grel.z + cosf(time_s * 2.1f + (float)p) * 1.2f};
            draw_plasma_beam(rr, vp, tip, weld, 0.08f, (RtsVec4f){1.0f, 0.82f, 0.28f, 0.96f}, time_s);
        }
    }
}

static void draw_wrist_pad_hud(RtsRetroRenderer *rr, RtsMat4f ortho, const RtsWorld *world, RtsVec3d cam_world) {
    if (!rr->active_gear || !rr->active_gear->wrist_pad_open) return;
    const RtsFieldGearState *fg = rr->active_gear;
    float px = 18.0f, py = 74.0f, pw = 420.0f, ph = 336.0f;

    hud_border_box(rr, ortho, px, py, pw, ph,
                   (RtsVec4f){0.02f, 0.06f, 0.13f, 0.92f}, (RtsVec4f){0.26f, 0.94f, 1.0f, 0.96f});

    /* Header + Comms Relay Link Status */
    RtsRadarContact contacts[RTS_RADAR_MAX_CONTACTS];
    bool comms_ok = true;
    uint32_t hq_id = (world->entity_count > 0u) ? world->entities[0].id : 0u;
    size_t ccnt = rts_field_gear_scan_radar(world, 1u, hq_id, cam_world, 450.0, contacts, RTS_RADAR_MAX_CONTACTS, &comms_ok);

    hud_text(rr, ortho, px + 12.0f, py + 10.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.34f, 1.0f}, "TACTICAL WRIST-PAD [TAB]");
    hud_text(rr, ortho, px + 266.0f, py + 10.0f, 1.0f,
             comms_ok ? (RtsVec4f){0.32f, 1.0f, 0.72f, 1.0f} : (RtsVec4f){1.0f, 0.32f, 0.42f, 1.0f},
             comms_ok ? "COMMS: LINKED" : "RELAY LOST");

    /* 4 Interactive Diegetic Tabs */
    static const char *const tab_lbl[4] = {"1 RADAR", "2 DECK", "3 FLEET", "4 ECO"};
    for (int t = 0; t < 4; ++t) {
        float tx = px + 10.0f + (float)t * 100.0f;
        bool active = ((int)fg->wrist_tab == t);
        hud_border_box(rr, ortho, tx, py + 28.0f, 94.0f, 22.0f,
                       active ? (RtsVec4f){0.10f, 0.30f, 0.48f, 0.96f} : (RtsVec4f){0.04f, 0.10f, 0.20f, 0.90f},
                       active ? (RtsVec4f){1.0f, 0.86f, 0.32f, 1.0f} : (RtsVec4f){0.26f, 0.68f, 0.88f, 0.75f});
        hud_text(rr, ortho, tx + 8.0f, py + 35.0f, 1.0f,
                 active ? (RtsVec4f){1.0f, 0.96f, 0.62f, 1.0f} : (RtsVec4f){0.76f, 0.90f, 1.0f, 1.0f}, tab_lbl[t]);
    }

    float cy = py + 62.0f;
    if (fg->wrist_tab == RTS_WPAD_RADAR) {
        float rx = px + 14.0f, ry = cy, rw = 216.0f, rh = 216.0f;
        hud_border_box(rr, ortho, rx, ry, rw, rh, (RtsVec4f){0.01f, 0.04f, 0.09f, 0.96f}, (RtsVec4f){0.22f, 0.86f, 0.96f, 0.88f});
        hud_quad(rr, ortho, rx + rw * 0.5f, ry, 1.0f, rh, (RtsVec4f){0.18f, 0.50f, 0.66f, 0.45f});
        hud_quad(rr, ortho, rx, ry + rh * 0.5f, rw, 1.0f, (RtsVec4f){0.18f, 0.50f, 0.66f, 0.45f});
        for (size_t i = 0; i < ccnt; ++i) {
            float bx = rx + rw * 0.5f + contacts[i].rel_x_norm * (rw * 0.44f);
            float by = ry + rh * 0.5f + contacts[i].rel_z_norm * (rh * 0.44f);
            RtsVec4f bc = (contacts[i].faction_id == 2u) ? (RtsVec4f){1.0f, 0.28f, 0.42f, 1.0f}
                        : (contacts[i].orbital ? (RtsVec4f){1.0f, 0.84f, 0.30f, 1.0f} : (RtsVec4f){0.28f, 0.98f, 1.0f, 1.0f});
            hud_quad(rr, ortho, bx - 4.0f, by - 4.0f, 8.0f, 8.0f, bc);
        }
        hud_quad(rr, ortho, rx + rw * 0.5f - 3.0f, ry + rh * 0.5f - 3.0f, 6.0f, 6.0f, (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f});

        /* Right-column live Fog-of-War Contact Legend */
        hud_text(rr, ortho, rx + rw + 12.0f, ry + 4.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.36f, 1.0f}, "HORIZON CONTACTS");
        size_t shown = ccnt < 10u ? ccnt : 10u;
        for (size_t i = 0; i < shown; ++i) {
            char line[32];
            upper_trunc_copy(line, sizeof(line), contacts[i].label, 20);
            RtsVec4f lc = (contacts[i].faction_id == 2u) ? (RtsVec4f){1.0f, 0.42f, 0.52f, 1.0f} : (RtsVec4f){0.78f, 0.96f, 1.0f, 1.0f};
            hud_text(rr, ortho, rx + rw + 12.0f, ry + 24.0f + (float)i * 18.0f, 1.0f, lc, line);
        }
    } else if (fg->wrist_tab == RTS_WPAD_DECK_MAP) {
        char buf[96];
        if (rr->active_mega && rr->active_mega->total_rooms > 0u) {
            snprintf(buf, sizeof(buf), "VESSEL DECK %u / %u (%u ROOMS)",
                     (unsigned)(rr->active_mega->active_deck + 1u), (unsigned)rr->active_mega->deck_count,
                     (unsigned)rr->active_mega->total_rooms);
        } else {
            snprintf(buf, sizeof(buf), "SURFACE EVA - NO INTERIOR ACTIVE");
        }
        hud_text(rr, ortho, px + 14.0f, cy + 8.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.36f, 1.0f}, buf);
        snprintf(buf, sizeof(buf), "LIVE STAFF: %zu | REPAIRS: %u | BREACHERS: %u",
                 rr->active_crew ? rr->active_crew->count : 0u,
                 rr->active_crew ? (unsigned)rr->active_crew->repairs_completed : 0u,
                 rr->active_crew ? (unsigned)rr->active_crew->breachers_neutralized : 0u);
        hud_text(rr, ortho, px + 14.0f, cy + 32.0f, 1.0f, (RtsVec4f){0.34f, 0.98f, 0.90f, 1.0f}, buf);
        hud_text(rr, ortho, px + 14.0f, cy + 60.0f, 1.0f, (RtsVec4f){0.86f, 0.94f, 1.0f, 1.0f}, "USE [/] FOR TURBOLIFT OR N FOR NEXT SHIP");
    } else if (fg->wrist_tab == RTS_WPAD_FLEET_ROSTER) {
        hud_text(rr, ortho, px + 14.0f, cy + 4.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.36f, 1.0f}, "ACTIVE SOLARIS FLEET ROSTER");
        int row = 0;
        for (size_t i = 0; i < world->entity_count && row < 11; ++i) {
            const RtsEntity *e = &world->entities[i];
            if (!e->active || e->faction_id != 1u) continue;
            char row_txt[72];
            int hp_pct = e->max_hull > 0.0f ? (int)(100.0f * e->hull / e->max_hull) : 100;
            snprintf(row_txt, sizeof(row_txt), "%.24s | HP:%3d%% | %s", e->name, hp_pct, e->orbital_mode ? "LEO" : "SRF");
            char up[64];
            upper_trunc_copy(up, sizeof(up), row_txt, 48);
            hud_text(rr, ortho, px + 14.0f, cy + 26.0f + (float)row * 20.0f, 1.0f, (RtsVec4f){0.84f, 0.96f, 1.0f, 1.0f}, up);
            row++;
        }
    } else {
        char buf[96];
        double ore = rr->active_eco ? rr->active_eco->ledgers[1].ore : world->resources_ore;
        double fuel = rr->active_eco ? rr->active_eco->ledgers[1].fuel : world->resources_fuel;
        snprintf(buf, sizeof(buf), "ORE: %.0f | FUEL: %.0f | PWR: +%.0fMW", ore, fuel, world->resources_power);
        hud_text(rr, ortho, px + 14.0f, cy + 8.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.36f, 1.0f}, buf);
        hud_text(rr, ortho, px + 14.0f, cy + 32.0f, 1.0f, (RtsVec4f){0.34f, 0.98f, 0.92f, 1.0f}, "ACTIVE 1:1 GANTRY PRODUCTION QUEUE:");
        int row = 0;
        if (rr->active_eco) {
            for (size_t i = 0; i < rr->active_eco->job_count && row < 6; ++i) {
                const RtsProductionJob *jb = &rr->active_eco->jobs[i];
                if (!jb->active || jb->completed) continue;
                float pr = jb->total_time_s > 0.01 ? rts_clampf((float)(1.0 - jb->remaining_s / jb->total_time_s), 0.0f, 1.0f) : 1.0f;
                char jt[64];
                snprintf(jt, sizeof(jt), "#%u %.22s %3d%%", jb->id, jb->blueprint_id, (int)(pr * 100.0f));
                char up[64];
                upper_trunc_copy(up, sizeof(up), jt, 44);
                float ry = cy + 58.0f + (float)row * 34.0f;
                hud_text(rr, ortho, px + 14.0f, ry, 1.0f, (RtsVec4f){0.94f, 0.98f, 1.0f, 1.0f}, up);
                hud_border_box(rr, ortho, px + 14.0f, ry + 14.0f, pw - 28.0f, 10.0f,
                               (RtsVec4f){0.04f, 0.10f, 0.18f, 0.95f}, (RtsVec4f){0.26f, 0.92f, 1.0f, 0.88f});
                hud_quad(rr, ortho, px + 16.0f, ry + 16.0f, (pw - 32.0f) * pr, 6.0f, (RtsVec4f){1.0f, 0.84f, 0.30f, 1.0f});
                row++;
            }
        }
        if (row == 0) {
            hud_text(rr, ortho, px + 14.0f, cy + 60.0f, 1.0f, (RtsVec4f){0.74f, 0.86f, 0.98f, 1.0f}, "NO ACTIVE JOBS - ACCESS FOUNDRY TERMINAL [E]");
        }
    }

    hud_text(rr, ortho, px + 14.0f, py + ph - 18.0f, 1.0f, (RtsVec4f){0.78f, 0.90f, 1.0f, 1.0f}, "KEYS 1-4: SWITCH TAB | TAB: CLOSE WRIST-PAD");
}

static void draw_optics_hud(RtsRetroRenderer *rr, RtsMat4f ortho) {
    if (!rr->active_gear || rr->active_gear->optics_level <= 0) return;
    const RtsFieldGearState *fg = rr->active_gear;
    float w = (float)rr->width, h = (float)rr->height;
    float cx = w * 0.5f, cy = h * 0.5f;

    /* Sleek Optical Binoculars Framing Aperture Bars & Cyan Mil-Dot Rangefinder Reticle */
    hud_quad(rr, ortho, 0.0f, 64.0f, w, 24.0f, (RtsVec4f){0.01f, 0.03f, 0.07f, 0.80f});
    hud_quad(rr, ortho, 0.0f, h - 86.0f, w, 24.0f, (RtsVec4f){0.01f, 0.03f, 0.07f, 0.80f});
    hud_border_box(rr, ortho, cx - 220.0f, cy - 150.0f, 440.0f, 300.0f,
                   (RtsVec4f){0.0f, 0.0f, 0.0f, 0.0f}, (RtsVec4f){0.24f, 0.94f, 1.0f, 0.82f});

    /* Center Precision Crosshair + Mil-Dot Ticks */
    hud_quad(rr, ortho, cx - 90.0f, cy - 1.0f, 68.0f, 2.0f, (RtsVec4f){0.28f, 0.98f, 1.0f, 0.92f});
    hud_quad(rr, ortho, cx + 22.0f, cy - 1.0f, 68.0f, 2.0f, (RtsVec4f){0.28f, 0.98f, 1.0f, 0.92f});
    hud_quad(rr, ortho, cx - 1.0f, cy - 72.0f, 2.0f, 50.0f, (RtsVec4f){0.28f, 0.98f, 1.0f, 0.92f});
    hud_quad(rr, ortho, cx - 1.0f, cy + 22.0f, 2.0f, 50.0f, (RtsVec4f){0.28f, 0.98f, 1.0f, 0.92f});
    hud_quad(rr, ortho, cx - 3.0f, cy - 3.0f, 6.0f, 6.0f, fg->paint_active ? (RtsVec4f){1.0f, 0.34f, 0.42f, 1.0f} : (RtsVec4f){1.0f, 0.88f, 0.30f, 1.0f});

    char opt_hdr[96];
    snprintf(opt_hdr, sizeof(opt_hdr), "OPTICAL BINOCULARS %dX | FOV %.1f DEG | LASER DESIGNATOR READY",
             rts_field_gear_optics_zoom_x(fg), rts_field_gear_fov_deg(fg));
    hud_text(rr, ortho, cx - 220.0f, 70.0f, 1.0f, (RtsVec4f){0.36f, 0.98f, 1.0f, 1.0f}, opt_hdr);

    if (fg->paint_active) {
        char ptxt[96];
        snprintf(ptxt, sizeof(ptxt), "LASER PAINTED: %.48s | [LMB] CALL STRIKE", fg->painted_label);
        char pup[88];
        upper_trunc_copy(pup, sizeof(pup), ptxt, 74);
        hud_text(rr, ortho, cx - 220.0f, h - 80.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.30f, 1.0f}, pup);
    }
}

static void draw_radial_wheel_hud(RtsRetroRenderer *rr, RtsMat4f ortho) {
    if (!rr->active_gear || !rr->active_gear->radial_open) return;
    const RtsFieldGearState *fg = rr->active_gear;
    float cx = (float)rr->width * 0.5f;
    float cy = (float)rr->height * 0.5f;
    float rad_x = 278.0f, rad_y = 205.0f;

    /* Place banner above radial ring so center target & optical reticle remain 100% unobstructed */
    hud_border_box(rr, ortho, cx - 122.0f, cy - rad_y - 54.0f, 244.0f, 26.0f,
                   (RtsVec4f){0.03f, 0.08f, 0.16f, 0.94f}, (RtsVec4f){1.0f, 0.84f, 0.30f, 0.98f});
    hud_text(rr, ortho, cx - 108.0f, cy - rad_y - 46.0f, 1.0f, (RtsVec4f){1.0f, 0.88f, 0.36f, 1.0f}, "RADIAL QUICK ORDERS [1-7]");

    for (int i = 0; i < RTS_RADIAL_COUNT; ++i) {
        double ang = -RTS_PI * 0.5 + (2.0 * RTS_PI * (double)i) / (double)RTS_RADIAL_COUNT;
        float bx = cx + (float)cos(ang) * rad_x - 78.0f;
        float by = cy + (float)sin(ang) * rad_y - 16.0f;
        bool hi = ((int)fg->radial_hovered == i);
        hud_border_box(rr, ortho, bx, by, 156.0f, 32.0f,
                       hi ? (RtsVec4f){0.14f, 0.32f, 0.50f, 0.96f} : (RtsVec4f){0.03f, 0.07f, 0.15f, 0.92f},
                       hi ? (RtsVec4f){1.0f, 0.86f, 0.30f, 1.0f} : (RtsVec4f){0.26f, 0.90f, 1.0f, 0.90f});
        char lbl[32];
        snprintf(lbl, sizeof(lbl), "[%d] %s", i + 1, rts_radial_order_name((RtsRadialOrder)i));
        hud_text(rr, ortho, bx + 10.0f, by + 10.0f, 1.0f,
                 hi ? (RtsVec4f){1.0f, 0.96f, 0.52f, 1.0f} : (RtsVec4f){0.92f, 0.98f, 1.0f, 1.0f}, lbl);
    }
}

static void draw_facility_console_hud(RtsRetroRenderer *rr, RtsMat4f ortho, const RtsWorld *world) {
    if (!rr->active_gear || !rr->active_gear->facility_open) return;
    const RtsFieldGearState *fg = rr->active_gear;
    float w = (float)rr->width, h = (float)rr->height;
    float bw = 940.0f, bh = 128.0f;
    float bx = w * 0.5f - bw * 0.5f, by = h - 188.0f;

    hud_border_box(rr, ortho, bx, by, bw, bh,
                   (RtsVec4f){0.02f, 0.07f, 0.15f, 0.94f}, (RtsVec4f){1.0f, 0.82f, 0.30f, 0.98f});

    char hdr[128];
    snprintf(hdr, sizeof(hdr), "TIER 2 PRODUCTION TERMINAL: %.38s [PRESS 1-6 TO QUEUE | E EXIT]", fg->facility_name);
    char hdr_up[128];
    upper_trunc_copy(hdr_up, sizeof(hdr_up), hdr, 108);
    hud_text(rr, ortho, bx + 14.0f, by + 10.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.34f, 1.0f}, hdr_up);

    size_t show = fg->facility_item_count < 6u ? fg->facility_item_count : 6u;
    for (size_t i = 0; i < show; ++i) {
        const RtsConstructBlueprint *bp = rts_world_find_blueprint(world, fg->facility_items[i]);
        double ore = 0.0, fuel = 0.0, bs = 0.0;
        if (bp) rts_economy_blueprint_cost(bp, &ore, &fuel, &bs);
        int col = (int)(i % 3u), row = (int)(i / 3u);
        float ix = bx + 14.0f + (float)col * 306.0f;
        float iy = by + 32.0f + (float)row * 42.0f;
        bool sel = ((int)i == fg->facility_selected_idx);
        hud_border_box(rr, ortho, ix, iy, 298.0f, 36.0f,
                       sel ? (RtsVec4f){0.10f, 0.28f, 0.44f, 0.95f} : (RtsVec4f){0.04f, 0.10f, 0.20f, 0.90f},
                       sel ? (RtsVec4f){1.0f, 0.86f, 0.32f, 1.0f} : (RtsVec4f){0.24f, 0.88f, 1.0f, 0.84f});
        char item_txt[64];
        snprintf(item_txt, sizeof(item_txt), "[%u] %.28s", (unsigned)(i + 1u), bp ? bp->name : fg->facility_items[i]);
        char iup[48];
        upper_trunc_copy(iup, sizeof(iup), item_txt, 33);
        hud_text(rr, ortho, ix + 8.0f, iy + 6.0f, 1.0f, (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, iup);
        char cost_txt[48];
        snprintf(cost_txt, sizeof(cost_txt), "%.0f ORE | %.0f FUEL | %.0fS", ore, fuel, bs);
        hud_text(rr, ortho, ix + 8.0f, iy + 21.0f, 1.0f, (RtsVec4f){0.36f, 0.98f, 0.90f, 1.0f}, cost_txt);
    }
}

static RtsVertex g_planet_verts[2400];

static void draw_spherical_planet_surface(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world,
                                          RtsVec3d cam_world, double alt_m, RtsVec3f light, float time_s) {
    RtsQuatd idq = rts_quat_identity();
    RtsPlanetLodSpec lod = rts_planet_select_lod(alt_m);
    if (alt_m <= 80000.0) {
        size_t tcnt = rts_planet_build_surface_patch(&world->planet, cam_world, lod, g_planet_verts, 2400u);
        if (tcnt >= 3u) {
            rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, g_planet_verts, tcnt);
            draw_mesh(rr, rr->sh_mesh, rr->mesh_dynamic_3d, RTS_GFX_INVALID, vp, rts_mat4_identity(),
                      (RtsVec4f){0.96f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
        size_t ocnt = rts_planet_build_ocean_patch(&world->planet, cam_world, lod, time_s, g_planet_verts, 2400u);
        if (ocnt >= 3u) {
            rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, g_planet_verts, ocnt);
            draw_mesh(rr, rr->sh_mesh, rr->mesh_dynamic_3d, rr->tex_ocean, vp, rts_mat4_identity(),
                      (RtsVec4f){0.92f, 0.98f, 1.0f, 0.88f}, light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
        }
        if (lod.render_clouds) {
            size_t ccnt = rts_planet_build_cloud_patch(&world->planet, cam_world, time_s, g_planet_verts, 2400u);
            if (ccnt >= 3u) {
                rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, g_planet_verts, ccnt);
                draw_mesh(rr, rr->sh_holo, rr->mesh_dynamic_3d, RTS_GFX_INVALID, vp, rts_mat4_identity(),
                          (RtsVec4f){0.90f, 0.96f, 1.0f, 0.28f}, light, time_s, true, false, false, false, RTS_DRAW_TRIANGLES);
            }
        }
    } else {
        RtsQuatd orb_rot = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.72),
                                        rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 1.35 + time_s * 0.01));
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(orb_rot, (RtsVec3f){-95.0f, -150.0f, -260.0f}, (RtsVec3f){195.0f, 195.0f, 195.0f}),
                  (RtsVec4f){0.88f, 0.95f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(orb_rot, (RtsVec3f){-95.0f, -150.0f, -260.0f}, (RtsVec3f){202.0f, 202.0f, 202.0f}),
                  (RtsVec4f){0.24f, 0.88f, 1.0f, 0.30f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        (void)idq;
    }
}

static void draw_live_combat_vfx(RtsRetroRenderer *rr, RtsMat4f vp, const RtsWorld *world,
                                 RtsVec3d cam_world, RtsVec3f light, float time_s) {
    if (!rr->active_combat) return;
    const RtsCombatSim *cs = rr->active_combat;
    RtsQuatd idq = rts_quat_identity();

    /* Live 64-bit Projectiles, Beam Lances, Torpedoes, AA Flak, Orbital Ion & EMP Pulses */
    for (size_t i = 0; i < RTS_COMBAT_MAX_PROJ; ++i) {
        const RtsLiveProjectile *p = &cs->projs[i];
        if (!p->active) continue;
        RtsVec3f prel = rts_v3d_relative_f(p->pos, cam_world);
        if (prel.x * prel.x + prel.y * prel.y + prel.z * prel.z > 2400.0f * 2400.0f) continue;

        RtsVec4f col = (p->faction_id == 2u) ? (RtsVec4f){1.0f, 0.24f, 0.40f, 1.0f} : (RtsVec4f){0.22f, 0.96f, 1.0f, 1.0f};
        float width = 0.22f;
        if (p->kind == RTS_WPN_EMP_PULSE) { col = (RtsVec4f){0.62f, 0.46f, 1.0f, 1.0f}; width = 0.36f; }
        else if (p->kind == RTS_WPN_ORBITAL_ION) { col = (RtsVec4f){0.36f, 0.96f, 1.0f, 1.0f}; width = 0.72f; }
        else if (p->kind == RTS_WPN_BEAM_LANCE) { col = (RtsVec4f){1.0f, 0.84f, 0.28f, 1.0f}; width = 0.44f; }
        else if (p->kind == RTS_WPN_HOMING_TORPEDO) { col = (RtsVec4f){1.0f, 0.56f, 0.18f, 1.0f}; width = 0.32f; }

        if (p->kind == RTS_WPN_BEAM_LANCE || p->kind == RTS_WPN_ORBITAL_ION) {
            RtsVec3f orel = rts_v3d_relative_f(p->origin, cam_world);
            draw_plasma_beam(rr, vp, orel, prel, width, col, time_s);
        } else {
            RtsVec3d tail_w = rts_v3d_sub(p->pos, rts_v3d_scale(rts_v3d_norm(p->vel), 5.5));
            draw_plasma_beam(rr, vp, rts_v3d_relative_f(tail_w, cam_world), prel, width, col, time_s);
        }
    }

    /* Directional Hex-Shield Impact Ripples & EMP-Primed Electric Halos */
    for (size_t i = 0; i < RTS_COMBAT_MAX_RIPPLES; ++i) {
        const RtsShieldRipple *rp = &cs->ripples[i];
        if (!rp->active) continue;
        const RtsEntity *e = rts_world_get_entity_const(world, rp->entity_id);
        if (!e) continue;
        RtsVec3f erel = rts_v3d_relative_f(e->pos, cam_world);
        float rad = fmaxf(fmaxf(e->dimensions_m.x, e->dimensions_m.z) * 0.34f, 5.2f);
        RtsVec4f scol = rp->emp_tint ? (RtsVec4f){0.64f, 0.44f, 1.0f, 0.52f * rp->intensity}
                                     : (RtsVec4f){0.22f, 0.94f, 1.0f, 0.50f * rp->intensity};
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, erel, (RtsVec3f){rad, rad * 0.72f, rad}),
                  scol, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
    }

    for (size_t i = 0; i < world->entity_count && i < RTS_MAX_ENTITIES; ++i) {
        const RtsEntity *e = &world->entities[i];
        if (!e->active || cs->emp_prime_timer[i] <= 0.0f) continue;
        RtsVec3f erel = rts_v3d_relative_f(e->pos, cam_world);
        float rad = fmaxf(fmaxf(e->dimensions_m.x, e->dimensions_m.z) * 0.42f, 6.2f);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 3.4),
                                         (RtsVec3f){erel.x, erel.y + 1.8f, erel.z}, (RtsVec3f){rad, rad, rad}),
                  (RtsVec4f){0.72f, 0.52f, 1.0f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    }

    /* Carved Destructible Voxel Terrain Crater / Asteroid Patch */
    if (cs->voxel_patch_initialized && cs->crater_count > 0u) {
        size_t vc = rts_voxel_build_mesh(&cs->destructible_patch, (RtsVoxelVertex *)g_planet_verts, 2400u);
        if (vc >= 3u) {
            rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, g_planet_verts, vc);
            RtsVec3f crel = rts_v3d_relative_f(cs->craters[0].world_pos, cam_world);
            draw_mesh(rr, rr->sh_mesh, rr->mesh_dynamic_3d, RTS_GFX_INVALID, vp,
                      rts_mat4_from_quat_trs(idq, (RtsVec3f){crel.x - 16.0f, crel.y - 6.0f, crel.z - 16.0f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                      (RtsVec4f){0.92f, 0.94f, 0.98f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
    }

    /* Plasma Re-Entry Heat Shield Envelope When Piloting Spacecraft in Upper Atmosphere */
    if (rr->active_vphys && rr->active_vphys->reentry_glow > 0.05f) {
        const RtsEntity *pe = rts_world_get_entity_const(world, rr->active_vphys->entity_id);
        if (pe) {
            RtsVec3f prel = rts_v3d_relative_f(pe->pos, cam_world);
            float g = rr->active_vphys->reentry_glow;
            draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                      rts_mat4_from_quat_trs(pe->rot, (RtsVec3f){prel.x, prel.y - 1.2f, prel.z}, (RtsVec3f){9.5f, 4.2f, 11.5f}),
                      (RtsVec4f){1.0f, 0.44f + 0.30f * g, 0.14f, 0.55f * g}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        }
    }
}

static void draw_vehicle_physics_hud(RtsRetroRenderer *rr, RtsMat4f ortho, const RtsWorld *world) {
    if (!rr->active_vphys) return;
    const RtsVehiclePhysicsState *vp = rr->active_vphys;
    if (vp->entity_id == 0u && vp->time_compression <= 1 && vp->reentry_glow <= 0.02f) return;
    const RtsEntity *e = rts_world_get_entity_const(world, vp->entity_id);

    float bw = 720.0f, bh = 38.0f;
    float bx = (float)rr->width * 0.5f - bw * 0.5f, by = 64.0f;
    hud_border_box(rr, ortho, bx, by, bw, bh,
                   (RtsVec4f){0.02f, 0.07f, 0.15f, 0.90f},
                   (vp->reentry_glow > 0.1f) ? (RtsVec4f){1.0f, 0.46f, 0.20f, 0.98f} : (RtsVec4f){0.26f, 0.94f, 1.0f, 0.94f});

    char line[160];
    if (vp->reentry_glow > 0.05f) {
        snprintf(line, sizeof(line), "6-DOF ORBITAL RE-ENTRY | VEL %.0FM/S | HEAT %.0FKW/M2 | WARP %dX",
                 vp->prograde_dv_mps, vp->reentry_heat_wm2 * 0.001, vp->time_compression);
    } else if (e && e->construct_class == RTS_CLASS_AIRCRAFT) {
        snprintf(line, sizeof(line), "VTOL %.0f%% | AIRSPEED %.0FM/S | AGL %.0FM | CARGO BAY:%s | WARP %dX",
                 vp->vtol_blend * 100.0, vp->airspeed_mps, vp->altitude_agl_m,
                 vp->clamped_cargo_id ? "CLAMPED" : "OPEN", vp->time_compression);
    } else if (e && e->construct_class == RTS_CLASS_NAVAL_SHIP) {
        snprintf(line, sizeof(line), "%s | DEPTH %.1FM | ROLL %.1f DEG | WAKE %.0f%% | WARP %dX",
                 vp->is_submarine ? "SUBMARINE BALLAST" : "NAVAL BUOYANCY",
                 vp->ballast_depth_m, vp->wave_roll_rad * 57.3, (double)vp->wake_intensity * 100.0, vp->time_compression);
    } else if (e && e->construct_class == RTS_CLASS_GROUND_VEHICLE) {
        snprintf(line, sizeof(line), "HOVER %.1FM | SLOPE %.1f DEG | TURRET %.0f DEG | WARP %dX",
                 vp->hover_clearance_m, vp->slope_pitch_rad * 57.3, vp->turret_yaw_rad * 57.3, vp->time_compression);
    } else {
        snprintf(line, sizeof(line), "TIME COMPRESSION %dX %s| 6-DOF FLIGHT TELEMETRY READY",
                 vp->time_compression, vp->time_warp_blocked_combat ? "(COMBAT LOCK 1X) " : "");
    }
    char lup[128];
    upper_trunc_copy(lup, sizeof(lup), line, 86);
    hud_text(rr, ortho, bx + 14.0f, by + 13.0f, 1.0f,
             (vp->reentry_glow > 0.1f) ? (RtsVec4f){1.0f, 0.72f, 0.30f, 1.0f} : (RtsVec4f){0.42f, 0.98f, 0.96f, 1.0f}, lup);
}

void rts_renderer_render_live_world(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, float time_s) {
    if (!rr || !rr->initialized || !world) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    float fov_deg = rr->active_gear ? rts_field_gear_fov_deg(rr->active_gear) : 62.0f;
    RtsMat4f proj = rts_mat4_perspective((float)((double)fov_deg * RTS_DEG2RAD), aspect, 0.2f, 3500.0f);
    RtsVec3f sun = {0.62f, 0.36f, -0.70f};
    RtsVec3f light = {0.58f, 0.78f, 0.38f};
    RtsQuatd idq = rts_quat_identity();

    RtsVec3d cam_world = rts_v3d(0.0, world->planet.radius_m, 0.0);
    rts_interior_local_to_world(&world->interiors, world->commander.frame_id, world->commander.local_pos, &cam_world);
    double alt_m = rts_v3d_len(cam_world) - world->planet.radius_m;
    if (alt_m < 0.0) alt_m = 0.0;
    bool in_orbit = (alt_m > 80000.0);
    bool red_alert = (rr->active_crew && rr->active_crew->red_alert);

    RtsVec3f fwd = {
        (float)(sin(world->commander.yaw_rad) * cos(world->commander.pitch_rad)),
        (float)(sin(world->commander.pitch_rad)),
        (float)(-cos(world->commander.yaw_rad) * cos(world->commander.pitch_rad))
    };
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at((RtsVec3f){0, 0, 0}, fwd, (RtsVec3f){0, 1, 0}));
    rr->cur_cam_pos = (RtsVec3f){0, 0, 0};
    rr->cur_fog_color = red_alert ? (RtsVec4f){0.26f, 0.06f, 0.10f, 0.38f}
                      : (in_orbit ? (RtsVec4f){0.04f, 0.09f, 0.18f, 0.16f} : (RtsVec4f){0.22f, 0.34f, 0.46f, 0.48f});

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height,
                        in_orbit ? (RtsVec4f){0.02f, 0.02f, 0.06f, 1.0f} : (RtsVec4f){0.04f, 0.09f, 0.18f, 1.0f});
    draw_sky_dome(rr, vp, (RtsVec3f){0, 0, 0}, sun, red_alert ? 2.0f : (in_orbit ? 1.0f : 0.0f), time_s);

    /* Continuous Multi-LOD Spherical Planetary Terrain, Animated Ocean & Orbital Cloud Deck */
    draw_spherical_planet_surface(rr, vp, world, cam_world, alt_m, light, time_s);

    /* Render Hydrated Multi-Room Interior Architecture, Ceilings, Doorways, Props, Consoles & All Staffed 12-Role Crew */
    bool inside_construct = (world->commander.frame_id != world->planet_frame_id &&
                             world->commander.occupied_entity_id != 0u);
    if (inside_construct) {
        RtsVec3d fpos = rts_v3d(0, 0, 0);
        RtsQuatd frot = idq;
        rts_interior_frame_world_pose(&world->interiors, world->commander.frame_id, &fpos, &frot);
        double cmd_deck_y = floor(world->commander.local_pos.y / 3.8) * 3.8;

        for (size_t r = 0; r < world->interiors.room_count; ++r) {
            const RtsInteriorRoom *rm = &world->interiors.rooms[r];
            if (!rm->active || rm->frame_id != world->commander.frame_id) continue;
            if (fabs(rm->local_bounds.min.y - cmd_deck_y) > 1.8) continue;
            double cx = 0.5 * (rm->local_bounds.min.x + rm->local_bounds.max.x);
            double cz = 0.5 * (rm->local_bounds.min.z + rm->local_bounds.max.z);
            double rw = rm->local_bounds.max.x - rm->local_bounds.min.x;
            double rl = rm->local_bounds.max.z - rm->local_bounds.min.z;
            RtsVec4f floor_tint = (rm->damaged_pct > 1.0f) ? (RtsVec4f){1.0f, 0.55f, 0.52f, 1.0f} : (RtsVec4f){0.88f, 0.94f, 1.0f, 1.0f};
            RtsVec4f wall_tint = red_alert ? (RtsVec4f){0.92f, 0.62f, 0.66f, 1.0f} : (RtsVec4f){0.80f, 0.88f, 0.98f, 1.0f};

            RtsVec3d floor_w;
            rts_interior_local_to_world(&world->interiors, rm->frame_id, rts_v3d(cx, rm->local_bounds.min.y - 0.15, cz), &floor_w);
            draw_box(rr, vp, rts_v3d_relative_f(floor_w, cam_world), (RtsVec3f){(float)rw, 0.30f, (float)rl}, frot, rr->tex_deck_floor,
                     floor_tint, light, time_s);

            /* Enclosed overhead Solaris ceiling deck slab with glowing ceiling light batten */
            if (rm->kind != RTS_ROOM_GENERIC || rm->door_mask != 0u) {
                RtsVec3d ceil_w;
                rts_interior_local_to_world(&world->interiors, rm->frame_id, rts_v3d(cx, rm->local_bounds.min.y + 3.92, cz), &ceil_w);
                RtsVec4f ceil_tint = red_alert ? (RtsVec4f){0.42f, 0.14f, 0.18f, 1.0f} : (RtsVec4f){0.28f, 0.38f, 0.54f, 1.0f};
                draw_box(rr, vp, rts_v3d_relative_f(ceil_w, cam_world), (RtsVec3f){(float)rw, 0.24f, (float)rl}, frot, rr->tex_deck_floor,
                         ceil_tint, light, time_s);
                RtsVec3d strip_w;
                rts_interior_local_to_world(&world->interiors, rm->frame_id, rts_v3d(cx, rm->local_bounds.min.y + 3.76, cz), &strip_w);
                RtsVec4f strip_c = red_alert ? (RtsVec4f){1.0f, 0.26f, 0.36f, 1.0f} : (RtsVec4f){0.38f, 0.94f, 1.0f, 1.0f};
                draw_box(rr, vp, rts_v3d_relative_f(strip_w, cam_world), (RtsVec3f){(float)rw * 0.52f, 0.08f, 0.42f}, frot, rr->tex_crt_console,
                         strip_c, light, time_s);
            }

            /* Orient vaulted Solaris arch ribs accurately inside each room (interior_arch.obj is 20.44m x 10.23m) */
            if (rm->kind != RTS_ROOM_TURBOLIFT) {
                RtsVec3d arch_w;
                rts_interior_local_to_world(&world->interiors, rm->frame_id, rts_v3d(cx, rm->local_bounds.min.y, cz), &arch_w);
                bool corr_rot = (rm->kind == RTS_ROOM_CORRIDOR || rm->kind == RTS_ROOM_TRAM_STATION);
                RtsQuatd arot = corr_rot ? rts_quat_mul(frot, rts_quat_from_axis_angle(rts_v3d(0, 1, 0), RTS_PI * 0.5)) : frot;
                float span_m = corr_rot ? (float)rl : (float)rw;
                float asc_x = rts_clampf(span_m / 22.2f, 0.22f, 0.65f);
                draw_mesh(rr, rr->sh_mesh, rr->mesh_arch, rr->tex_hull_blue, vp,
                          rts_mat4_from_quat_trs(arot, rts_v3d_relative_f(arch_w, cam_world), (RtsVec3f){asc_x, 0.376f, 0.72f}),
                          wall_tint, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
            }

            /* Doorway-pierced bulkheads (only drawn on mega-rooms with door_mask or non-generic kind) */
            if (rm->kind != RTS_ROOM_GENERIC || rm->door_mask != 0u) {
                bool pano = (rm->kind == RTS_ROOM_WAR_ROOM || rm->kind == RTS_ROOM_BRIDGE_CIC || rm->kind == RTS_ROOM_HANGAR);
                draw_room_edge_wall(rr, vp, world, rm->frame_id, frot, cam_world,
                                    rts_v3d(cx, rm->local_bounds.min.y, rm->local_bounds.min.z),
                                    (float)rw, true, (rm->door_mask & RTS_DOOR_N) != 0u, pano, wall_tint, light, time_s);
                draw_room_edge_wall(rr, vp, world, rm->frame_id, frot, cam_world,
                                    rts_v3d(rm->local_bounds.min.x, rm->local_bounds.min.y, cz),
                                    (float)rl, false, (rm->door_mask & RTS_DOOR_W) != 0u, pano, wall_tint, light, time_s);
                if ((rm->door_mask & RTS_DOOR_S) == 0u) {
                    draw_room_edge_wall(rr, vp, world, rm->frame_id, frot, cam_world,
                                        rts_v3d(cx, rm->local_bounds.min.y, rm->local_bounds.max.z),
                                        (float)rw, true, false, pano, wall_tint, light, time_s);
                }
                if ((rm->door_mask & RTS_DOOR_E) == 0u) {
                    draw_room_edge_wall(rr, vp, world, rm->frame_id, frot, cam_world,
                                        rts_v3d(rm->local_bounds.max.x, rm->local_bounds.min.y, cz),
                                        (float)rl, false, false, pano, wall_tint, light, time_s);
                }
                draw_room_specific_props(rr, vp, world, rm, frot, cam_world, light, time_s);
            }
        }

        /* Physical Interactive Consoles (Holo-Globe, Factory Terminal, Pilot Seat, Escape Pod, Gunner, Turbolift) */
        for (size_t c = 0; c < world->interiors.console_count; ++c) {
            const RtsConsoleStation *cs = &world->interiors.consoles[c];
            if (!cs->active || cs->frame_id != world->commander.frame_id) continue;
            RtsVec3d cw;
            rts_interior_local_to_world(&world->interiors, cs->frame_id, cs->local_pos, &cw);
            RtsVec3f crel = rts_v3d_relative_f(cw, cam_world);

            if (cs->type == RTS_CONSOLE_HOLO_GLOBE) {
                double hyaw = rr->active_holo ? rr->active_holo->yaw_rad : (double)(time_s * 0.45f);
                double hpitch = rr->active_holo ? rr->active_holo->pitch_rad : 0.0;
                RtsQuatd spin = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), hyaw),
                                             rts_quat_from_axis_angle(rts_v3d(1, 0, 0), hpitch));
                draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_hull_blue, vp,
                          rts_mat4_from_quat_trs(frot, (RtsVec3f){crel.x, crel.y - 0.85f, crel.z}, (RtsVec3f){1.65f, 0.45f, 1.65f}),
                          (RtsVec4f){0.42f, 0.56f, 0.76f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
                draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                          rts_mat4_from_quat_trs(spin, (RtsVec3f){crel.x, crel.y + 0.55f, crel.z}, (RtsVec3f){0.92f, 0.92f, 0.92f}),
                          (RtsVec4f){0.55f, 0.96f, 1.0f, 0.92f}, light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
                draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                          rts_mat4_from_quat_trs(spin, (RtsVec3f){crel.x, crel.y + 0.55f, crel.z}, (RtsVec3f){1.02f, 1.02f, 1.02f}),
                          (RtsVec4f){0.22f, 0.96f, 1.0f, 0.50f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
                draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                          rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.36), (RtsVec3f){crel.x, crel.y + 0.55f, crel.z}, (RtsVec3f){1.42f, 1.42f, 1.42f}),
                          (RtsVec4f){1.0f, 0.84f, 0.30f, 0.96f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

                /* Live Interactive Tier 3 Holo-Globe Blips, Selection Halos, Order Arcs & Placement Ghost */
                RtsVec3d gcenter_local = rts_v3d(cs->local_pos.x, cs->local_pos.y + 0.55, cs->local_pos.z);
                draw_hologlobe_3d_projections(rr, vp, world, cs->frame_id, gcenter_local, cam_world, light, time_s);
            } else {
                RtsVec4f glow = (cs->type == RTS_CONSOLE_ESCAPE_POD) ? (RtsVec4f){0.28f, 1.0f, 0.74f, 1.0f}
                              : ((cs->type == RTS_CONSOLE_TURBOLIFT) ? (RtsVec4f){1.0f, 0.84f, 0.30f, 1.0f}
                              : ((cs->type == RTS_CONSOLE_FACTORY_TERMINAL) ? (RtsVec4f){1.0f, 0.82f, 0.32f, 1.0f}
                              : (RtsVec4f){0.28f, 0.94f, 1.0f, 1.0f}));
                draw_box(rr, vp, (RtsVec3f){crel.x, crel.y - 0.35f, crel.z}, (RtsVec3f){1.3f, 1.1f, 0.8f}, frot,
                         rr->tex_crt_console, glow, light, time_s);
            }
        }

        /* Render All Live Staffed 12-Role Crew Agents (or fallback single officer if no active crew sim) */
        if (rr->active_crew && rr->active_crew->entity_id == world->commander.occupied_entity_id && rr->active_crew->count > 0u) {
            for (size_t i = 0; i < rr->active_crew->count; ++i) {
                const RtsCrewAgent *a = &rr->active_crew->agents[i];
                if (!a->active) continue;
                double bob = (a->state == RTS_CREW_AT_STATION) ? 0.0 : (fabs(sin((double)a->anim_phase)) * 0.06);
                RtsVec3d aw;
                rts_interior_local_to_world(&world->interiors, world->commander.frame_id,
                                            rts_v3d(a->local_pos.x, a->local_pos.y + bob, a->local_pos.z), &aw);
                RtsVec3f arel = rts_v3d_relative_f(aw, cam_world);
                RtsGfxHandle ctex = (a->faction_id == 2u) ? rr->tex_hull_red : rr->tex_hull_blue;
                RtsVec4f ctint = rts_crew_role_tint(a->role, a->faction_id);
                draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, ctex, vp,
                          rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), a->yaw_rad), arel, (RtsVec3f){1.06f, 1.06f, 1.06f}),
                          ctint, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

                double tdist = rts_v3d_dist(a->local_pos, a->target_pos);
                if ((a->state == RTS_CREW_REPAIRING && tdist < 3.8) || (a->state == RTS_CREW_COMBAT && tdist < 7.0)) {
                    RtsVec3d tw;
                    rts_interior_local_to_world(&world->interiors, world->commander.frame_id,
                                                rts_v3d(a->target_pos.x, a->target_pos.y + 1.45, a->target_pos.z), &tw);
                    RtsVec3f trel = rts_v3d_relative_f(tw, cam_world);
                    RtsVec4f bcol = (a->state == RTS_CREW_REPAIRING) ? (RtsVec4f){1.0f, 0.86f, 0.28f, 0.95f}
                                  : ((a->faction_id == 2u) ? (RtsVec4f){1.0f, 0.24f, 0.42f, 0.95f} : (RtsVec4f){0.24f, 0.96f, 1.0f, 0.95f});
                    draw_plasma_beam(rr, vp, (RtsVec3f){arel.x, arel.y + 1.25f, arel.z}, trel, 0.036f, bcol, time_s);
                }
            }
        } else {
            RtsVec3d crw_w;
            rts_interior_local_to_world(&world->interiors, world->commander.frame_id, rts_v3d(-3.2, 0.0, -4.0), &crw_w);
            draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                      rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 0.6), rts_v3d_relative_f(crw_w, cam_world), (RtsVec3f){1.05f, 1.05f, 1.05f}),
                      (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        }
    }

    /* Render Every Live World Construct Using Its Own Dedicated Unique Sculpted .OBJ Mesh! */
    for (size_t i = 0; i < world->entity_count; ++i) {
        const RtsEntity *e = &world->entities[i];
        if (!e->active) continue;
        if (inside_construct && e->id == world->commander.occupied_entity_id) continue;

        RtsVec3f rel = rts_v3d_relative_f(e->pos, cam_world);
        if (rel.x * rel.x + rel.y * rel.y + rel.z * rel.z > 3200.0f * 3200.0f) continue;

        const RtsConstructBlueprint *bp = rts_world_find_blueprint(world, e->blueprint_id);
        const RtsMeshCacheEntry *me = bp ? rts_renderer_get_mesh_entry(rr, bp->mesh_path) : NULL;
        RtsGfxHandle mesh = me ? me->handle : rr->mesh_citadel;
        float sc = 4.0f;
        if (me) {
            float target_len = fmaxf(e->dimensions_m.x, fmaxf(e->dimensions_m.y, e->dimensions_m.z));
            sc = rts_clampf((target_len * 0.26f) / me->bounds_radius, 1.4f, 14.0f);
        }
        RtsGfxHandle tex = (e->faction_id == 2) ? rr->tex_hull_red : rr->tex_hull_blue;
        RtsMat4f model = rts_mat4_from_quat_trs(e->rot, rel, (RtsVec3f){sc, sc, sc});
        draw_mesh(rr, rr->sh_mesh, mesh, tex, vp, model, (RtsVec4f){1, 1, 1, 1}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    }

    /* Tier 2 Live 1:1 Progressive Gantry Assembly for Active Production Jobs */
    draw_live_gantry_assemblies(rr, vp, world, cam_world, light, time_s);

    /* Unified 64-Bit Live Combat VFX (Projectiles, Beam Lances, Shield Ripples, EMP Rings, Voxel Craters & Re-entry Cone) */
    draw_live_combat_vfx(rr, vp, world, cam_world, light, time_s);

    /* Tier 1b Live 3D Laser Designator Beacon Spire in World Space */
    if (rr->active_gear && rr->active_gear->paint_active) {
        RtsVec3f lrel = rts_v3d_relative_f(rr->active_gear->painted_world_pos, cam_world);
        draw_plasma_beam(rr, vp, (RtsVec3f){lrel.x, lrel.y, lrel.z}, (RtsVec3f){lrel.x, lrel.y + 42.0f, lrel.z},
                         0.28f, (RtsVec4f){1.0f, 0.38f, 0.32f, 0.96f}, time_s);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 2.2), (RtsVec3f){lrel.x, lrel.y + 1.4f, lrel.z}, (RtsVec3f){5.2f, 5.2f, 5.2f}),
                  (RtsVec4f){1.0f, 0.84f, 0.28f, 0.98f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
    }

    /* Seamless Sandbox RTS HUD: Minimap + Tier 1/2/3 Diegetic Toolkit + 6-Class Flight Telemetry */
    float w = (float)rr->width, h = (float)rr->height;
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);

    if (inside_construct) {
        draw_deck_blueprint_minimap(rr, ortho, world, world->commander.frame_id);
    }
    draw_wrist_pad_hud(rr, ortho, world, cam_world);
    draw_optics_hud(rr, ortho);
    draw_radial_wheel_hud(rr, ortho);
    draw_facility_console_hud(rr, ortho, world);
    draw_vehicle_physics_hud(rr, ortho, world);

    const RtsEntity *occ = rts_world_get_entity_const(world, world->commander.occupied_entity_id);
    const RtsInteriorRoom *room = rts_interior_find_room_at(&world->interiors, world->commander.frame_id, world->commander.local_pos);
    const RtsConsoleStation *near_c = rts_interior_find_nearest_console(&world->interiors, world->commander.frame_id, world->commander.local_pos);

    char loc_banner[256];
    if (occ && room) {
        snprintf(loc_banner, sizeof(loc_banner), "%.44s - %.36s [ALT %.1fKM]", occ->name, room->name, alt_m / 1000.0);
    } else if (occ) {
        snprintf(loc_banner, sizeof(loc_banner), "%.48s [INTERIOR DECK | ALT %.1fKM]", occ->name, alt_m / 1000.0);
    } else {
        snprintf(loc_banner, sizeof(loc_banner), "HELIOS PRIME SURFACE [ALT %.1fKM]", alt_m / 1000.0);
    }
    char loc_upper[128];
    upper_trunc_copy(loc_upper, sizeof(loc_upper), loc_banner, 84);

    char sub_line[220];
    if (inside_construct && rr->active_mega && rr->active_mega->entity_id == world->commander.occupied_entity_id) {
        snprintf(sub_line, sizeof(sub_line), "%.42s | DECK %u/%u | %u ROOMS | %u CREW | ORE:%.0f",
                 rr->live_mode_tag[0] ? rr->live_mode_tag : "EXPLORABLE MEGA-INTERIOR",
                 (unsigned)(rr->active_mega->active_deck + 1u), (unsigned)rr->active_mega->deck_count,
                 (unsigned)rr->active_mega->total_rooms, (unsigned)rr->active_mega->total_crew_capacity,
                 world->resources_ore);
    } else {
        snprintf(sub_line, sizeof(sub_line), "%.64s | ORE:%.0f FUEL:%.0f PWR:%.0fMW | ENTITIES:%zu",
                 rr->live_mode_tag[0] ? rr->live_mode_tag : "SEAMLESS SANDBOX RTS",
                 world->resources_ore, world->resources_fuel, world->resources_power, world->entity_count);
    }

    /* Tier 3 Interactive Holo-Globe Command Bar (or Console Interaction Prompt) */
    if (world->commander.at_holo_globe) {
        hud_border_box(rr, ortho, w * 0.5f - 410.0f, h - 150.0f, 820.0f, 92.0f,
                       (RtsVec4f){0.02f, 0.07f, 0.15f, 0.93f}, (RtsVec4f){0.26f, 0.96f, 1.0f, 0.98f});
        if (rr->active_holo) {
            const RtsHoloGlobeState *hg = rr->active_holo;
            char top_bar[160];
            snprintf(top_bar, sizeof(top_bar), "3D HOLO-GLOBE | VIEW %.0fKM | SEL:%zu | MODE:%s | [E] EXIT",
                     rts_hologlobe_view_altitude_km(hg), hg->selected_count, rts_hologlobe_mode_name(hg->mode));
            char top_up[128];
            upper_trunc_copy(top_up, sizeof(top_up), top_bar, 96);
            hud_text(rr, ortho, w * 0.5f - 394.0f, h - 138.0f, 1.0f, (RtsVec4f){1.0f, 0.86f, 0.32f, 1.0f}, top_up);
            hud_text(rr, ortho, w * 0.5f - 394.0f, h - 114.0f, 1.0f, (RtsVec4f){0.94f, 0.98f, 1.0f, 1.0f},
                     "[1]SELECT [2]MOVE [3]ATTACK [4]ORBIT [5]DOCK [6]REENTRY [7]BOARD [X]BUILD GHOST [Q/R]ZOOM");
            char st_bar[160];
            if (hg->mode == RTS_HOLO_MODE_PLACE_BUILDING) {
                snprintf(st_bar, sizeof(st_bar), "GHOST: %.26s -> %.46s [LMB CONFIRM]", hg->place_blueprint_id, hg->ghost_reason);
            } else {
                snprintf(st_bar, sizeof(st_bar), "%.92s", hg->last_feedback);
            }
            char st_up[128];
            upper_trunc_copy(st_up, sizeof(st_up), st_bar, 96);
            RtsVec4f scol = (hg->mode == RTS_HOLO_MODE_PLACE_BUILDING && !hg->ghost_valid)
                ? (RtsVec4f){1.0f, 0.36f, 0.44f, 1.0f} : (RtsVec4f){0.34f, 0.98f, 0.90f, 1.0f};
            hud_text(rr, ortho, w * 0.5f - 394.0f, h - 88.0f, 1.0f, scol, st_up);
        } else {
            hud_text(rr, ortho, w * 0.5f - 394.0f, h - 134.0f, 1.10f, (RtsVec4f){1.0f, 0.84f, 0.30f, 1.0f},
                     "HOLO-GLOBE COMMAND LINK ACTIVE [PRESS E TO STEP BACK]");
            hud_text(rr, ortho, w * 0.5f - 394.0f, h - 102.0f, 1.04f, (RtsVec4f){0.94f, 0.98f, 1.0f, 1.0f},
                     "[1] QUEUE TANK  [2] ATTACK RAIDER  [3] ASTROGATOR 400KM ORBIT  [4] BOARD DREADNOUGHT");
        }
    } else if ((!rr->active_gear || !rr->active_gear->facility_open) && (rr->live_prompt[0] != '\0' || near_c != NULL)) {
        char pbuf[160];
        if (rr->live_prompt[0] != '\0') {
            snprintf(pbuf, sizeof(pbuf), "%.120s", rr->live_prompt);
        } else {
            snprintf(pbuf, sizeof(pbuf), "PRESS [E] TO ACCESS %.64s", near_c->name);
        }
        char pup[128];
        upper_trunc_copy(pup, sizeof(pup), pbuf, 70);
        hud_border_box(rr, ortho, w * 0.5f - 240.0f, h - 104.0f, 480.0f, 36.0f,
                       (RtsVec4f){0.03f, 0.08f, 0.16f, 0.88f}, (RtsVec4f){1.0f, 0.84f, 0.30f, 0.96f});
        hud_text(rr, ortho, w * 0.5f - 224.0f, h - 91.0f, 1.12f, (RtsVec4f){1.0f, 0.92f, 0.44f, 1.0f}, pup);
    }

    const char *toast = rr->live_toast[0]
        ? rr->live_toast
        : "WASD: WALK | TAB: WRIST-PAD | Z: OPTICS | V: RADIAL | E: CONSOLE/HOLO-GLOBE | [/]: LIFT | N: SHIP";
    render_hud_common(rr, world, input, loc_upper, sub_line, toast, false);
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

void rts_renderer_render_menu_screen(RtsRetroRenderer *rr, const RtsWorld *world, const RtsMenuState *menu, float time_s) {
    if (!rr || !rr->initialized || !menu) return;
    float w = (float)rr->width, h = (float)rr->height;
    float aspect = w / (h > 0.0f ? h : 1.0f);
    RtsMat4f proj = rts_mat4_perspective((float)(58.0 * RTS_DEG2RAD), aspect, 0.2f, 2200.0f);
    RtsVec3f cam = {0.0f, 10.0f, 44.0f};
    RtsVec3f sun = {0.66f, 0.34f, -0.66f};
    RtsVec3f light = {0.62f, 0.72f, 0.34f};
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 3.0f, -20.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));
    rr->cur_cam_pos = cam;
    rr->cur_fog_color = (RtsVec4f){0.03f, 0.06f, 0.14f, 0.18f};

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.015f, 0.025f, 0.065f, 1.0f});
    draw_sky_dome(rr, vp, cam, sun, 1.0f, time_s);

    /* Live 3D Rotating Helios Prime Planet + Atmospheric Rim + Orbiting Shipyard Alpha & Dreadnought Solaris */
    RtsQuatd p_rot = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.32),
                                  rts_quat_from_axis_angle(rts_v3d(0, 1, 0), (double)time_s * 0.12 + 0.8));
    draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
              rts_mat4_from_quat_trs(p_rot, (RtsVec3f){48.0f, -24.0f, -110.0f}, (RtsVec3f){64.0f, 64.0f, 64.0f}),
              (RtsVec4f){0.86f, 0.94f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(p_rot, (RtsVec3f){48.0f, -24.0f, -110.0f}, (RtsVec3f){67.5f, 67.5f, 67.5f}),
              (RtsVec4f){0.22f, 0.90f, 1.0f, 0.34f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(1, 0, 0), 0.42),
                                     (RtsVec3f){48.0f, -24.0f, -110.0f}, (RtsVec3f){92.0f, 92.0f, 92.0f}),
              (RtsVec4f){1.0f, 0.84f, 0.30f, 0.90f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

    RtsQuatd sy_rot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), (double)time_s * 0.16 - 0.5);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_shipyard, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(sy_rot, (RtsVec3f){-32.0f, 12.0f, -34.0f}, (RtsVec3f){1.9f, 1.9f, 1.9f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 0.72),
                                     (RtsVec3f){26.0f + sinf(time_s * 0.4f) * 3.0f, 15.0f, -26.0f}, (RtsVec3f){1.45f, 1.45f, 1.45f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), -0.45),
                                     (RtsVec3f){-14.0f + cosf(time_s * 0.9f) * 6.0f, -4.0f, -8.0f}, (RtsVec3f){1.1f, 1.1f, 1.1f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

    /* Solaris Retro-Futurism Glass Menu Panel (Matches rts_menu_click Hit Geometry 1:1) */
    RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
    int cnt = rts_menu_item_count(menu);
    float pw = 640.0f;
    float ph = 152.0f + (float)cnt * 48.0f;
    float px = w * 0.5f - pw * 0.5f;
    float py = h * 0.5f - 210.0f;

    RtsVec4f border_col = (menu->screen == RTS_MENU_MATCH_END && !menu->victory)
                        ? (RtsVec4f){1.0f, 0.28f, 0.42f, 1.0f} : (RtsVec4f){0.26f, 0.94f, 1.0f, 0.98f};
    hud_border_box(rr, ortho, px, py, pw, ph, (RtsVec4f){0.02f, 0.055f, 0.13f, 0.90f}, border_col);

    const char *title = rts_menu_screen_title(menu->screen);
    hud_text(rr, ortho, px + 24.0f, py + 20.0f, 1.55f, (RtsVec4f){1.0f, 0.86f, 0.34f, 1.0f}, title);

    char sub[128];
    if (menu->screen == RTS_MENU_SCENARIOS) {
        snprintf(sub, sizeof(sub), "%.78s", rts_scenario_brief(menu->selected_row < 4 ? menu->selected_row : 0));
    } else if (menu->screen == RTS_MENU_MATCH_END) {
        snprintf(sub, sizeof(sub), "%.78s", menu->match_summary[0] ? menu->match_summary : "OPERATION CONCLUDED");
    } else {
        snprintf(sub, sizeof(sub), "EXCLUSIVELY FIRST-PERSON RTS | 76 ENTERABLE CONSTRUCTS | IRONMAN: %s",
                 menu->skirmish_ironman ? "ON" : "OFF");
    }
    char sub_up[128];
    upper_trunc_copy(sub_up, sizeof(sub_up), sub, 76);
    hud_text(rr, ortho, px + 24.0f, py + 54.0f, 1.0f, (RtsVec4f){0.42f, 0.96f, 0.96f, 1.0f}, sub_up);

    for (int i = 0; i < cnt; ++i) {
        float ry = py + 86.0f + (float)i * 48.0f;
        bool sel = (i == menu->selected_row);
        hud_border_box(rr, ortho, px + 20.0f, ry, pw - 40.0f, 40.0f,
                       sel ? (RtsVec4f){0.11f, 0.30f, 0.48f, 0.96f} : (RtsVec4f){0.04f, 0.09f, 0.19f, 0.88f},
                       sel ? (RtsVec4f){1.0f, 0.86f, 0.32f, 1.0f} : (RtsVec4f){0.24f, 0.72f, 0.90f, 0.78f});
        char lbl[128];
        rts_menu_item_label(menu, world, i, lbl, sizeof(lbl));
        char lup[128];
        upper_trunc_copy(lup, sizeof(lup), lbl, 64);
        hud_text(rr, ortho, px + 36.0f, ry + 13.0f, 1.12f,
                 sel ? (RtsVec4f){1.0f, 0.96f, 0.56f, 1.0f} : (RtsVec4f){0.92f, 0.97f, 1.0f, 1.0f}, lup);
    }

    char foot_up[128];
    upper_trunc_copy(foot_up, sizeof(foot_up), menu->status_note, 74);
    hud_text(rr, ortho, px + 24.0f, py + ph - 44.0f, 0.98f, (RtsVec4f){0.74f, 0.88f, 0.98f, 1.0f}, foot_up);
    hud_text(rr, ortho, px + 24.0f, py + ph - 22.0f, 0.92f, (RtsVec4f){1.0f, 0.84f, 0.34f, 1.0f},
             "CLICK ITEM OR USE UP/DOWN + ENTER | KEYS 1-6 QUICK SELECT | ESC BACK");

    rr->gfx.end_frame(rr->gfx.state);
}

bool rts_renderer_capture_png(RtsRetroRenderer *rr, const char *out_path) {
    if (!rr || !rr->initialized || !rr->gfx.capture_png) return false;
    return rr->gfx.capture_png(rr->gfx.state, out_path, rr->width, rr->height);
}
