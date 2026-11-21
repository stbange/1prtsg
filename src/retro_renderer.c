/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
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

static RtsGfxHandle load_png_tex(RtsRetroRenderer *rr, const char *png_path) {
    uint8_t *bytes = NULL;
    size_t sz = 0;
    if (!rts_vfs_read_bytes(rr->vfs, "assets", png_path, &bytes, &sz)) {
        return RTS_GFX_INVALID;
    }
    RtsImage img;
    RtsGfxHandle h = RTS_GFX_INVALID;
    if (rts_image_load_png_memory(bytes, sz, &img)) {
        h = rr->gfx.create_texture_rgba(rr->gfx.state, img.width, img.height, img.pixels, true);
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
    rr->sh_shield = load_shader_pair(rr, "shaders/shield.vert", "shaders/shield.frag");
    rr->sh_holo   = load_shader_pair(rr, "shaders/holo.vert",   "shaders/holo.frag");
    rr->sh_hud    = load_shader_pair(rr, "shaders/hud.vert",    "shaders/hud.frag");

    rr->tex_hull_blue      = load_png_tex(rr, "textures/hull_blue.png");
    rr->tex_hull_red       = load_png_tex(rr, "textures/hull_red.png");
    rr->tex_deck_floor     = load_png_tex(rr, "textures/deck_floor.png");
    rr->tex_planet_surface = load_png_tex(rr, "textures/planet_surface.png");
    rr->tex_ocean          = load_png_tex(rr, "textures/ocean_wave.png");
    rr->tex_crt_console    = load_png_tex(rr, "textures/crt_console.png");
    rr->tex_crew_sprite    = load_png_tex(rr, "sprites/crew_8dir.png");
    rr->tex_font_atlas     = load_png_tex(rr, "fonts/retro_font.png");

    rr->mesh_cube   = build_unit_cube(rr);
    rr->mesh_sphere = build_uv_sphere(rr, 18, 28);
    rr->mesh_ring   = build_orbit_ring(rr, 96);

    rr->mesh_citadel     = load_obj_mesh(rr, "models/citadel.obj",     (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_dropship    = load_obj_mesh(rr, "models/dropship.obj",    (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_tank        = load_obj_mesh(rr, "models/plasma_tank.obj", (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_dreadnought = load_obj_mesh(rr, "models/dreadnought.obj", (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_shipyard    = load_obj_mesh(rr, "models/shipyard.obj",    (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_starfighter = load_obj_mesh(rr, "models/starfighter.obj", (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_carrier     = load_obj_mesh(rr, "models/carrier.obj",     (RtsVec3f){1.0f, 1.0f, 1.0f});
    rr->mesh_crew3d      = load_obj_mesh(rr, "models/crew_3d.obj",     (RtsVec3f){1.0f, 1.0f, 1.0f});
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

static void draw_starfield(RtsRetroRenderer *rr, RtsMat4f vp, RtsVec3f cam_pos, float t) {
    RtsVertex verts[360 * 6];
    size_t c = 0;
    uint32_t s = 0x51A7F31Du;
    for (int i = 0; i < 360; ++i) {
        s = s * 1664525u + 1013904223u;
        float az = (float)(s & 0xFFFFu) / 65535.0f * (float)(2.0 * RTS_PI);
        s = s * 1664525u + 1013904223u;
        float el = ((float)(s & 0xFFFFu) / 65535.0f * 1.4f - 0.35f) * (float)(RTS_PI * 0.5);
        float dist = 480.0f;
        float x = cam_pos.x + cosf(el) * cosf(az) * dist;
        float y = cam_pos.y + sinf(el) * dist;
        float z = cam_pos.z + cosf(el) * sinf(az) * dist;
        float sz = (i % 9 == 0) ? 1.7f : 0.9f;
        float cr = (i % 7 == 0) ? 0.85f : 0.95f;
        float cg = (i % 5 == 0) ? 0.92f : 0.98f;
        float cb = 1.0f;
        RtsVertex q[4] = {
            {x - sz, y - sz, z, 0,0,1, 0,0, cr,cg,cb},
            {x + sz, y - sz, z, 0,0,1, 1,0, cr,cg,cb},
            {x + sz, y + sz, z, 0,0,1, 1,1, cr,cg,cb},
            {x - sz, y + sz, z, 0,0,1, 0,1, cr,cg,cb}
        };
        verts[c++] = q[0]; verts[c++] = q[1]; verts[c++] = q[2];
        verts[c++] = q[0]; verts[c++] = q[2]; verts[c++] = q[3];
    }
    rr->gfx.update_mesh(rr->gfx.state, rr->mesh_dynamic_3d, verts, c);
    RtsDrawCmd cmd;
    memset(&cmd, 0, sizeof(cmd));
    cmd.shader = rr->sh_holo;
    cmd.mesh = rr->mesh_dynamic_3d;
    cmd.texture = RTS_GFX_INVALID;
    cmd.model = rts_mat4_identity();
    cmd.mvp = vp;
    cmd.tint = (RtsVec4f){1.0f, 1.0f, 1.0f, 0.95f};
    cmd.time_s = t;
    cmd.blend_additive = true;
    cmd.depth_test = true;
    cmd.depth_write = false;
    rr->gfx.draw(rr->gfx.state, &cmd);
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

/* --- 2D HUD & Retro Bitmap Text Helpers (Screen Coordinates: Top-Left 0,0) --- */
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
    hud_quad(rr, ortho, x, y, w, h, fill);
    hud_quad(rr, ortho, x, y, w, 2.0f, edge);
    hud_quad(rr, ortho, x, y + h - 2.0f, w, 2.0f, edge);
    hud_quad(rr, ortho, x, y, 2.0f, h, edge);
    hud_quad(rr, ortho, x + w - 2.0f, y, 2.0f, h, edge);
}

static void hud_text(RtsRetroRenderer *rr, RtsMat4f ortho, float x, float y, float scale, RtsVec4f col, const char *text) {
    if (!text || rr->tex_font_atlas == RTS_GFX_INVALID) return;
    /* External retro_font.png is a 16x6 grid of ASCII 32..127 (8x8px glyphs -> 128x48 atlas) */
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

    /* Top Command Status Bar */
    hud_border_box(rr, ortho, 16.0f, 12.0f, w - 32.0f, 50.0f,
                   (RtsVec4f){0.03f, 0.06f, 0.14f, 0.82f}, (RtsVec4f){0.20f, 0.78f, 1.0f, 0.92f});
    hud_text(rr, ortho, 26.0f, 20.0f, 1.42f, (RtsVec4f){0.35f, 0.92f, 1.0f, 1.0f}, location_banner);
    hud_text(rr, ortho, 26.0f, 41.0f, 1.12f, (RtsVec4f){0.98f, 0.82f, 0.25f, 1.0f}, subtitle);

    char roster_buf[96];
    size_t bp_n = world ? world->blueprint_count : 76u;
    size_t role_n = world ? world->crew_role_count : 12u;
    snprintf(roster_buf, sizeof(roster_buf), "IRONMAN: ON | ROSTER: %zu UNITS / %zu ROLES", bp_n, role_n);
    hud_text(rr, ortho, w - 458.0f, 21.0f, 1.25f, (RtsVec4f){1.0f, 0.42f, 0.35f, 1.0f}, roster_buf);
    hud_text(rr, ortho, w - 458.0f, 41.0f, 1.12f, (RtsVec4f){0.65f, 0.95f, 0.75f, 1.0f}, "ORE:2500 FUEL:1200 PWR:+400MW SHIELD:100%");

    /* Center subtle 1st-person reticle */
    float cx = w * 0.5f, cy = h * 0.5f;
    hud_quad(rr, ortho, cx - 10.0f, cy - 1.0f, 6.0f, 2.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.85f});
    hud_quad(rr, ortho, cx + 4.0f,  cy - 1.0f, 6.0f, 2.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.85f});
    hud_quad(rr, ortho, cx - 1.0f,  cy - 10.0f, 2.0f, 6.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.85f});
    hud_quad(rr, ortho, cx - 1.0f,  cy + 4.0f,  2.0f, 6.0f, (RtsVec4f){0.35f, 0.95f, 1.0f, 0.85f});

    /* Bottom Radio Comms Strip */
    if (!force_touch) {
        hud_border_box(rr, ortho, 16.0f, h - 52.0f, w - 32.0f, 36.0f,
                       (RtsVec4f){0.03f, 0.06f, 0.14f, 0.80f}, (RtsVec4f){0.98f, 0.78f, 0.22f, 0.88f});
        hud_text(rr, ortho, 28.0f, h - 40.0f, 1.38f, (RtsVec4f){0.96f, 0.96f, 1.0f, 1.0f}, radio_line);
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
        /* Virtual Thumbstick Base + Knob */
        float r = tl.stick_radius;
        hud_border_box(rr, ortho, tl.stick_cx - r, tl.stick_cy - r, r * 2.0f, r * 2.0f,
                       (RtsVec4f){0.05f, 0.14f, 0.28f, 0.45f}, (RtsVec4f){0.25f, 0.85f, 1.0f, 0.92f});
        float kx = input ? input->stick_knob_x : (tl.stick_cx + r * 0.35f);
        float ky = input ? input->stick_knob_y : (tl.stick_cy - r * 0.25f);
        hud_border_box(rr, ortho, kx - 26.0f, ky - 26.0f, 52.0f, 52.0f,
                       (RtsVec4f){0.20f, 0.78f, 1.0f, 0.75f}, (RtsVec4f){1.0f, 0.92f, 0.35f, 1.0f});
        hud_text(rr, ortho, tl.stick_cx - 42.0f, tl.stick_cy - r - 20.0f, 1.4f, (RtsVec4f){0.35f, 0.92f, 1.0f, 1.0f}, "MOVE STICK");

        /* Right Tactical Cluster Buttons */
        for (int i = 0; i < RTS_TBTN_COUNT; ++i) {
            RtsRectf rc = tl.buttons[i];
            RtsVec4f edge = (i == RTS_TBTN_INTERACT) ? (RtsVec4f){1.0f, 0.82f, 0.22f, 0.95f} : (RtsVec4f){0.25f, 0.85f, 1.0f, 0.90f};
            hud_border_box(rr, ortho, rc.x, rc.y, rc.w, rc.h, (RtsVec4f){0.06f, 0.14f, 0.28f, 0.62f}, edge);
            hud_text(rr, ortho, rc.x + 16.0f, rc.y + rc.h * 0.36f, 1.6f, (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, tl.labels[i]);
        }

        /* Mobile Touch Mode Badge + Radio Bar above controls */
        hud_border_box(rr, ortho, w * 0.30f, h - 52.0f, w * 0.38f, 36.0f,
                       (RtsVec4f){0.04f, 0.10f, 0.20f, 0.82f}, (RtsVec4f){0.30f, 0.95f, 0.65f, 0.95f});
        hud_text(rr, ortho, w * 0.315f, h - 39.0f, 1.35f, (RtsVec4f){0.40f, 1.0f, 0.72f, 1.0f}, "AUTO-TOUCH ACTIVE | PINCH/TAP HOLO-GLOBE");
    }
}

void rts_renderer_render_scene(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, RtsShowcaseScene scene, float time_s) {
    if (!rr || !rr->initialized) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    RtsMat4f proj = rts_mat4_perspective((float)(62.0 * RTS_DEG2RAD), aspect, 0.2f, 1600.0f);
    RtsVec3f light = {0.55f, 0.80f, 0.35f};
    RtsQuatd idq = rts_quat_identity();

    if (scene == RTS_SCENE_WAR_ROOM_HOLOGLOBE || scene == RTS_SCENE_MOBILE_TOUCH_HUD) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.02f, 0.04f, 0.09f, 1.0f});
        RtsVec3f cam = {0.0f, 2.35f, 6.2f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 1.55f, -1.2f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        /* Starfield visible beyond armored War Room viewports */
        draw_starfield(rr, vp, cam, time_s);

        /* War Room Deck Floor, Ceiling Ribs, Armored Pillars & Side Consoles */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.25f, 0.0f}, (RtsVec3f){22.0f, 0.5f, 20.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.85f, 0.90f, 1.0f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 7.2f, 0.0f},   (RtsVec3f){22.0f, 0.6f, 20.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.45f, 0.52f, 0.68f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){-10.5f, 3.5f, 0.0f}, (RtsVec3f){0.8f, 7.2f, 20.0f}, idq, rr->tex_hull_blue, (RtsVec4f){0.55f, 0.68f, 0.90f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 10.5f, 3.5f, 0.0f}, (RtsVec3f){0.8f, 7.2f, 20.0f}, idq, rr->tex_hull_blue, (RtsVec4f){0.55f, 0.68f, 0.90f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 0.8f, -9.8f},  (RtsVec3f){22.0f, 1.6f, 0.8f}, idq, rr->tex_hull_blue, (RtsVec4f){0.45f, 0.60f, 0.88f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){-5.8f, 3.8f, -9.8f}, (RtsVec3f){1.0f, 6.8f, 1.0f},  idq, rr->tex_hull_blue, (RtsVec4f){0.88f, 0.72f, 0.24f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 5.8f, 3.8f, -9.8f}, (RtsVec3f){1.0f, 6.8f, 1.0f},  idq, rr->tex_hull_blue, (RtsVec4f){0.88f, 0.72f, 0.24f, 1.0f}, light, time_s);

        /* Exterior distant silhouette visible through the armored War Room window */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.55), (RtsVec3f){-14.0f, 7.5f, -38.0f}, (RtsVec3f){1.4f, 1.4f, 1.4f}),
                  (RtsVec4f){0.85f, 0.92f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.4), (RtsVec3f){24.0f, 16.0f, -85.0f}, (RtsVec3f){1.8f, 1.8f, 1.8f}),
                  (RtsVec4f){1.0f, 0.55f, 0.45f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Physical War Room Holo-Globe Pedestal & CRT Consoles */
        draw_box(rr, vp, (RtsVec3f){0.0f, 0.45f, -0.6f}, (RtsVec3f){3.6f, 0.9f, 3.6f}, idq, rr->tex_hull_blue, (RtsVec4f){0.30f, 0.45f, 0.72f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 0.92f, -0.6f}, (RtsVec3f){3.9f, 0.12f, 3.9f}, idq, rr->tex_hull_blue, (RtsVec4f){0.95f, 0.78f, 0.22f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){-6.2f, 0.8f, -2.0f}, (RtsVec3f){1.8f, 1.5f, 4.2f}, idq, rr->tex_hull_blue, (RtsVec4f){0.35f, 0.55f, 0.85f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 6.2f, 0.8f, -2.0f}, (RtsVec3f){1.8f, 1.5f, 4.2f}, idq, rr->tex_hull_blue, (RtsVec4f){0.35f, 0.55f, 0.85f, 1.0f}, light, time_s);

        /* Close-range Low-Poly 3D Crew Officers staffing the War Room */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.75), (RtsVec3f){-3.4f, 0.0f, -2.1f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){0.95f, 0.95f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.85), (RtsVec3f){ 3.5f, 0.0f, -1.8f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){1.0f, 0.90f, 0.55f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Glowing 3D Planetary & Orbital Holo-Globe floating above the pedestal */
        RtsQuatd spin = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), time_s * 0.35 + 0.6);
        RtsMat4f gm = rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.30f, -0.6f}, (RtsVec3f){1.15f, 1.15f, 1.15f});
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp, gm,
                  (RtsVec4f){0.35f, 0.88f, 1.0f, 0.88f}, light, time_s, true, false, true, false, RTS_DRAW_TRIANGLES);
        RtsMat4f gs = rts_mat4_from_quat_trs(spin, (RtsVec3f){0.0f, 2.30f, -0.6f}, (RtsVec3f){1.24f, 1.24f, 1.24f});
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp, gs,
                  (RtsVec4f){0.15f, 0.90f, 1.0f, 0.55f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        /* Keplerian Orbital Rings around the Holo-Globe */
        RtsQuatd orb1 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), 0.35), rts_quat_from_axis_angle(rts_v3d(0,0,1), 0.22));
        RtsQuatd orb2 = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(1,0,0), -0.52), rts_quat_from_axis_angle(rts_v3d(0,0,1), -0.45));
        RtsQuatd orb3 = rts_quat_from_axis_angle(rts_v3d(1,0,0), 1.05);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb1, (RtsVec3f){0.0f, 2.30f, -0.6f}, (RtsVec3f){1.65f, 1.65f, 1.65f}), (RtsVec4f){1.0f, 0.85f, 0.20f, 0.95f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb2, (RtsVec3f){0.0f, 2.30f, -0.6f}, (RtsVec3f){2.05f, 2.05f, 1.85f}), (RtsVec4f){1.0f, 0.25f, 0.30f, 0.95f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_ring, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(orb3, (RtsVec3f){0.0f, 2.30f, -0.6f}, (RtsVec3f){1.45f, 1.45f, 1.45f}), (RtsVec4f){0.25f, 1.0f, 0.72f, 0.90f}, light, time_s, false, true, false, false, RTS_DRAW_LINES);

        /* Glowing Orbiting Ship Icons on the Holo-Globe */
        draw_mesh(rr, rr->sh_holo, rr->mesh_cube, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(idq, (RtsVec3f){1.55f, 2.55f, -0.25f}, (RtsVec3f){0.14f, 0.08f, 0.24f}), (RtsVec4f){1.0f, 0.90f, 0.25f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_holo, rr->mesh_cube, RTS_GFX_INVALID, vp, rts_mat4_from_quat_trs(idq, (RtsVec3f){-1.80f, 2.85f, -0.95f}, (RtsVec3f){0.18f, 0.10f, 0.28f}), (RtsVec4f){1.0f, 0.22f, 0.25f, 1.0f}, light, time_s, false, true, false, false, RTS_DRAW_TRIANGLES);

        bool is_touch = (scene == RTS_SCENE_MOBILE_TOUCH_HUD);
        render_hud_common(rr, world, input,
                          is_touch ? "1STPRTS [MOBILE AUTO-TOUCH] - WAR ROOM" : "COMMAND CITADEL - WAR ROOM [3D HOLO-GLOBE]",
                          "SOLAR DIRECTORATE | HELIOS PRIME (R=6371KM) | KEPLERIAN ORBITS",
                          "XO: ORBITAL SHIPYARD ALPHA PASSING OVERHEAD IN 400KM LEO. CRIMSON CRUISER DETECTED!",
                          is_touch);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_PLANETARY_SURFACE) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.05f, 0.14f, 0.30f, 1.0f});
        RtsVec3f cam = {-26.0f, 8.5f, 44.0f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){6.0f, 12.0f, -18.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_starfield(rr, vp, cam, time_s);

        /* Curved Planetary Ground & Shimmering Ocean Horizon */
        draw_box(rr, vp, (RtsVec3f){0.0f, -1.0f, -20.0f}, (RtsVec3f){420.0f, 2.0f, 420.0f}, idq, rr->tex_planet_surface, (RtsVec4f){0.85f, 0.92f, 0.85f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){140.0f, -0.8f, -140.0f}, (RtsVec3f){320.0f, 1.8f, 220.0f}, idq, rr->tex_hull_blue, (RtsVec4f){0.14f, 0.42f, 0.82f, 1.0f}, light, time_s);

        /* Towering 1:1 Planetary Command Citadel with Shimmering Energy Shield Dome */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_citadel, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.25), (RtsVec3f){12.0f, 0.0f, -34.0f}, (RtsVec3f){16.0f, 16.0f, 16.0f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){12.0f, 2.0f, -34.0f}, (RtsVec3f){36.0f, 30.0f, 36.0f}),
                  (RtsVec4f){0.18f, 0.85f, 1.0f, 0.65f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        /* 1:1 Scale Troop/Cargo Dropship Parked on Tarmac */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.65), (RtsVec3f){-14.0f, 1.2f, 8.0f}, (RtsVec3f){3.6f, 3.6f, 3.6f}),
                  (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Plasma Heavy Tanks rolling past the Commander */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){-2.0f, 0.0f, 16.0f}, (RtsVec3f){2.4f, 2.4f, 2.4f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -2.4), (RtsVec3f){48.0f, 0.0f, 8.0f}, (RtsVec3f){2.5f, 2.5f, 2.5f}),
                  (RtsVec4f){1.0f, 0.80f, 0.78f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Optional Voxel Fortification Bunker Module */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_voxel_sample, rr->tex_deck_floor, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){-32.0f, 0.0f, -6.0f}, (RtsVec3f){1.3f, 1.3f, 1.3f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Sleek Angular Starfighters streaking overhead */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.7), (RtsVec3f){-18.0f, 28.0f, -22.0f}, (RtsVec3f){2.2f, 2.2f, 2.2f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -2.1), (RtsVec3f){28.0f, 34.0f, -40.0f}, (RtsVec3f){2.4f, 2.4f, 2.4f}),
                  (RtsVec4f){1.0f, 0.85f, 0.80f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Hybrid Crew Visuals: 3D close marine near Commander + 8-dir pixel sprites at mid-range */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.4), (RtsVec3f){-18.0f, 0.0f, 31.0f}, (RtsVec3f){1.4f, 1.4f, 1.4f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_billboard_sprite(rr, vp, (RtsVec3f){-8.0f, 0.0f, 22.0f}, cam, 2.6f, 3.4f, 1, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);
        draw_billboard_sprite(rr, vp, (RtsVec3f){-4.5f, 0.0f, 24.0f}, cam, 2.6f, 3.4f, 3, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);
        draw_billboard_sprite(rr, vp, (RtsVec3f){ 4.0f, 0.0f, 21.0f}, cam, 2.6f, 3.4f, 6, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);

        /* Plasma Bolts streaking between Crimson forces & Citadel Shield Dome */
        draw_plasma_beam(rr, vp, (RtsVec3f){45.0f, 3.2f, 7.0f}, (RtsVec3f){22.0f, 12.0f, -14.0f}, 0.45f, (RtsVec4f){1.0f, 0.25f, 0.18f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-1.0f, 3.4f, 15.0f}, (RtsVec3f){32.0f, 4.5f, 9.0f}, 0.40f, (RtsVec4f){0.20f, 0.88f, 1.0f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-16.0f, 28.0f, -22.0f}, (RtsVec3f){18.0f, 32.0f, -35.0f}, 0.35f, (RtsVec4f){0.35f, 1.0f, 0.45f, 1.0f}, time_s);

        render_hud_common(rr, world, input,
                          "HELIOS PRIME SURFACE - CITADEL [1:1 SCALE]",
                          "ALT:142M | SHIELD:94% | 3D CREW + 8-DIR SPRITES + VOXEL BUNKER",
                          "GUNNER: PLASMA TANKS ENGAGING CRIMSON VANGUARD! DROPSHIP VALKYRIE-2 READY FOR BOARDING.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_DROPSHIP_INTERIOR) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.015f, 0.025f, 0.07f, 1.0f});
        RtsVec3f cam = {0.0f, 2.0f, 5.8f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){0.0f, 1.9f, -14.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_starfield(rr, vp, cam, time_s);

        /* Illuminated Curved Planet & Escort Starfighter Visible Through Open Dropship Bay */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,0,1), 0.3), (RtsVec3f){14.0f, -68.0f, -135.0f}, (RtsVec3f){76.0f, 76.0f, 76.0f}),
                  (RtsVec4f){0.92f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){14.0f, -68.0f, -135.0f}, (RtsVec3f){79.5f, 79.5f, 79.5f}),
                  (RtsVec4f){0.22f, 0.75f, 1.0f, 0.58f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.45), (RtsVec3f){-4.2f, 4.4f, -26.0f}, (RtsVec3f){1.7f, 1.7f, 1.7f}),
                  (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){16.0f, 10.5f, -72.0f}, (RtsVec3f){2.8f, 2.8f, 2.8f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Dropship Walkable Troop Bay Interior (Local Reference Frame) */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.2f, 0.0f},   (RtsVec3f){7.2f, 0.4f, 16.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.92f, 0.94f, 1.0f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 4.4f, 0.0f},    (RtsVec3f){7.2f, 0.4f, 16.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.45f, 0.55f, 0.75f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){-3.6f, 2.1f, 0.0f},   (RtsVec3f){0.4f, 4.4f, 16.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.62f, 0.74f, 0.96f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 3.6f, 2.1f, 0.0f},   (RtsVec3f){0.4f, 4.4f, 16.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.62f, 0.74f, 0.96f, 1.0f}, light, time_s);
        for (int i = -1; i <= 1; ++i) {
            float rz = (float)i * 4.5f;
            draw_box(rr, vp, (RtsVec3f){-3.2f, 2.1f, rz}, (RtsVec3f){0.45f, 4.2f, 0.45f}, idq, rr->tex_hull_blue, (RtsVec4f){0.95f, 0.78f, 0.22f, 1.0f}, light, time_s);
            draw_box(rr, vp, (RtsVec3f){ 3.2f, 2.1f, rz}, (RtsVec3f){0.45f, 4.2f, 0.45f}, idq, rr->tex_hull_blue, (RtsVec4f){0.95f, 0.78f, 0.22f, 1.0f}, light, time_s);
        }
        /* Escort Marine Squad (3D models inside bay) */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.4), (RtsVec3f){-2.2f, 0.0f, 1.2f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -1.4), (RtsVec3f){ 2.2f, 0.0f, -1.6f}, (RtsVec3f){1.0f, 1.0f, 1.0f}),
                  (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Translucent Shimmering Containment Field across Open Bay */
        draw_mesh(rr, rr->sh_shield, rr->mesh_cube, RTS_GFX_INVALID, vp,
                  rts_mat4_from_quat_trs(idq, (RtsVec3f){0.0f, 2.1f, -7.8f}, (RtsVec3f){6.8f, 4.0f, 0.08f}),
                  (RtsVec4f){0.18f, 0.85f, 1.0f, 0.24f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

        render_hud_common(rr, world, input,
                          "DROPSHIP 'VALKYRIE-2' BAY - ORBITAL ASCENT",
                          "ALT:84.2KM | VEL:4,820 M/S | 1.0G ARTIFICIAL | ESCAPE POD ARMED",
                          "PILOT: CLEARING UPPER ATMOSPHERE! MAG-SHIELDS AT 98%. APPROACHING SHIPYARD ALPHA.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_NAVAL_CARRIER_DECK) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.06f, 0.22f, 0.45f, 1.0f});
        RtsVec3f cam = {-6.0f, 3.6f, 28.0f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){6.0f, 5.2f, -32.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        draw_starfield(rr, vp, cam, time_s);

        /* Vast Curved Retro Ocean Horizon */
        draw_box(rr, vp, (RtsVec3f){0.0f, -6.5f, -40.0f}, (RtsVec3f){520.0f, 2.0f, 520.0f}, idq, rr->tex_ocean, (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s);

        /* Standing 1:1 on the 340m Fleet Starfighter Carrier Flight Deck */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.4f, -18.0f}, (RtsVec3f){42.0f, 0.8f, 145.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.92f, 0.95f, 1.0f, 1.0f}, light, time_s);
        /* Starboard Island ATC Control Tower */
        draw_box(rr, vp, (RtsVec3f){18.0f, 9.0f, -14.0f}, (RtsVec3f){10.0f, 18.0f, 28.0f}, idq, rr->tex_hull_blue, (RtsVec4f){0.90f, 0.95f, 1.0f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){18.0f, 20.5f, -14.0f}, (RtsVec3f){8.0f, 5.5f, 16.0f}, idq, rr->tex_crt_console, (RtsVec4f){0.75f, 0.95f, 1.0f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){18.0f, 29.0f, -14.0f}, (RtsVec3f){1.6f, 12.0f, 1.6f}, idq, rr->tex_hull_blue, (RtsVec4f){0.96f, 0.82f, 0.25f, 1.0f}, light, time_s);

        /* Angular Starfighters staged & launching off the Carrier Flight Deck */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.15), (RtsVec3f){-8.0f, 1.6f, 4.0f}, (RtsVec3f){3.0f, 3.0f, 3.0f}),
                  (RtsVec4f){1,1,1,1}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.1), (RtsVec3f){-4.0f, 9.0f, -38.0f}, (RtsVec3f){3.2f, 3.2f, 3.2f}),
                  (RtsVec4f){1,1,1,1}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.35), (RtsVec3f){4.0f, 1.2f, -12.0f}, (RtsVec3f){3.2f, 3.2f, 3.2f}),
                  (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Escort Naval Carrier & Crimson Raider on the Horizon */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_carrier, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.18), (RtsVec3f){-68.0f, -4.5f, -72.0f}, (RtsVec3f){7.5f, 7.5f, 7.5f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.45), (RtsVec3f){64.0f, -3.5f, -110.0f}, (RtsVec3f){6.5f, 6.5f, 6.5f}),
                  (RtsVec4f){1.0f, 0.78f, 0.75f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Deck Crew directing Starfighter launch (3D up close + 8-dir sprite down deck) */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.8), (RtsVec3f){-1.5f, 0.0f, 14.0f}, (RtsVec3f){1.25f, 1.25f, 1.25f}),
                  (RtsVec4f){1.0f, 0.92f, 0.45f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_billboard_sprite(rr, vp, (RtsVec3f){6.5f, 0.0f, 8.0f}, cam, 2.2f, 3.0f, 2, 8, rr->tex_crew_sprite, (RtsVec4f){1,1,1,1}, time_s);

        /* Anti-Ship Plasma Volley & Shield Flash */
        draw_plasma_beam(rr, vp, (RtsVec3f){52.0f, 2.5f, -96.0f}, (RtsVec3f){10.0f, 12.0f, -18.0f}, 0.65f, (RtsVec4f){1.0f, 0.24f, 0.18f, 1.0f}, time_s);
        draw_plasma_beam(rr, vp, (RtsVec3f){-4.0f, 9.0f, -38.0f}, (RtsVec3f){54.0f, 1.5f, -102.0f}, 0.55f, (RtsVec4f){0.20f, 0.92f, 1.0f, 1.0f}, time_s);

        render_hud_common(rr, world, input,
                          "FLEET CARRIER 'VANGUARD' - 340M FLIGHT DECK",
                          "OCEAN SECTOR 4 | COMMS RELAY: LINKED | SCRAMBLING STARFIGHTER WING",
                          "ATC TOWER: INTERCEPTOR FLIGHT ALPHA AWAY! CRIMSON DESTROYER DETECTED ON HORIZON!",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    if (scene == RTS_SCENE_FOUNDRY_INTERIOR) {
        rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.025f, 0.03f, 0.06f, 1.0f});
        RtsVec3f cam = {-2.5f, 2.6f, 14.5f};
        RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){2.0f, 3.2f, -12.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

        /* Cavernous 1:1 Vehicle Foundry Assembly Hall */
        draw_box(rr, vp, (RtsVec3f){0.0f, -0.3f, 0.0f},   (RtsVec3f){36.0f, 0.6f, 44.0f}, idq, rr->tex_deck_floor, (RtsVec4f){0.88f, 0.92f, 1.0f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 16.5f, 0.0f},   (RtsVec3f){36.0f, 0.8f, 44.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.42f, 0.50f, 0.68f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){-17.5f, 8.0f, 0.0f},  (RtsVec3f){1.0f, 16.5f, 44.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.55f, 0.65f, 0.86f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){ 17.5f, 8.0f, 0.0f},  (RtsVec3f){1.0f, 16.5f, 44.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.55f, 0.65f, 0.86f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){0.0f, 8.0f, -21.5f},  (RtsVec3f){36.0f, 16.5f, 1.0f}, idq, rr->tex_hull_blue,  (RtsVec4f){0.40f, 0.52f, 0.72f, 1.0f}, light, time_s);

        /* Overhead Yellow Industrial Gantry Crane Beams */
        draw_box(rr, vp, (RtsVec3f){0.0f, 12.6f, -4.0f},  (RtsVec3f){34.0f, 1.2f, 1.6f},  idq, rr->tex_hull_blue, (RtsVec4f){0.98f, 0.78f, 0.18f, 1.0f}, light, time_s);
        draw_box(rr, vp, (RtsVec3f){2.0f, 9.2f, -4.0f},   (RtsVec3f){1.2f, 6.2f, 1.2f},   idq, rr->tex_hull_blue, (RtsVec4f){0.98f, 0.78f, 0.18f, 1.0f}, light, time_s);

        /* 1:1 Scale Plasma Heavy Tank Being Welded on Assembly Bay Floor */
        draw_mesh(rr, rr->sh_mesh, rr->mesh_tank, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.45), (RtsVec3f){2.5f, 0.2f, -4.5f}, (RtsVec3f){3.6f, 3.6f, 3.6f}),
                  (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        /* Welding plasma arc from overhead crane onto tank turret */
        draw_plasma_beam(rr, vp, (RtsVec3f){2.0f, 6.2f, -4.0f}, (RtsVec3f){2.4f, 4.1f, -4.3f}, 0.22f, (RtsVec4f){0.35f, 0.95f, 1.0f, 1.0f}, time_s);

        /* Foreman CRT Production Terminal & Engineer Crew */
        draw_box(rr, vp, (RtsVec3f){6.8f, 1.2f, 6.2f}, (RtsVec3f){2.8f, 2.2f, 1.2f}, rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.4), rr->tex_crt_console, (RtsVec4f){1,1,1,1}, light, time_s);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.8), (RtsVec3f){5.2f, 0.0f, 4.8f}, (RtsVec3f){1.15f, 1.15f, 1.15f}),
                  (RtsVec4f){1.0f, 0.92f, 0.55f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
        draw_mesh(rr, rr->sh_mesh, rr->mesh_crew3d, rr->tex_hull_blue, vp,
                  rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.7), (RtsVec3f){-4.2f, 0.0f, 1.8f}, (RtsVec3f){1.15f, 1.15f, 1.15f}),
                  (RtsVec4f){0.92f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

        /* Raised Diegetic Tactical Wrist-Pad on Left Forearm */
        float w = (float)rr->width, h = (float)rr->height;
        RtsMat4f ortho = rts_mat4_ortho(0.0f, w, h, 0.0f, -1.0f, 1.0f);
        hud_border_box(rr, ortho, 26.0f, h - 300.0f, 360.0f, 230.0f,
                       (RtsVec4f){0.02f, 0.08f, 0.14f, 0.92f}, (RtsVec4f){0.25f, 0.95f, 0.72f, 0.95f});
        hud_text(rr, ortho, 40.0f, h - 284.0f, 1.35f, (RtsVec4f){0.35f, 1.0f, 0.75f, 1.0f}, "TACTICAL WRIST-PAD [COMMS: OK]");
        hud_text(rr, ortho, 40.0f, h - 254.0f, 1.15f, (RtsVec4f){0.98f, 0.85f, 0.25f, 1.0f}, "PRODUCTION QUEUE:");
        hud_text(rr, ortho, 40.0f, h - 230.0f, 1.10f, (RtsVec4f){0.90f, 0.96f, 1.0f, 1.0f}, "1. PLASMA HEAVY TANK  [78%]");
        hud_text(rr, ortho, 40.0f, h - 208.0f, 1.10f, (RtsVec4f){0.75f, 0.88f, 1.0f, 1.0f}, "2. ARMORED APC        [QUEUED]");
        hud_text(rr, ortho, 40.0f, h - 186.0f, 1.10f, (RtsVec4f){0.75f, 0.88f, 1.0f, 1.0f}, "3. MOBILE LANDSHIP    [QUEUED]");
        hud_text(rr, ortho, 40.0f, h - 150.0f, 1.15f, (RtsVec4f){0.35f, 0.92f, 1.0f, 1.0f}, "SENSORS: RECON SAT LEO-1 PASS");
        hud_text(rr, ortho, 40.0f, h - 126.0f, 1.10f, (RtsVec4f){1.0f, 0.45f, 0.35f, 1.0f}, "ACOUSTICS: INTERIOR MUFFLE 65%");
        hud_quad(rr, ortho, 40.0f, h - 98.0f, 320.0f, 14.0f, (RtsVec4f){0.08f, 0.22f, 0.30f, 1.0f});
        hud_quad(rr, ortho, 40.0f, h - 98.0f, 250.0f, 14.0f, (RtsVec4f){0.25f, 0.95f, 0.72f, 1.0f});

        render_hud_common(rr, world, input,
                          "VEHICLE FOUNDRY - 1:1 ASSEMBLY HALL [INTERIOR]",
                          "QUEUE: PLASMA HEAVY TANK 78% | POWER: +400MW | WRIST-PAD OPEN",
                          "FOREMAN: TANK HULL WELDING COMPLETE IN 4 SECONDS. READY TO ROLL ONTO APRON.",
                          false);
        rr->gfx.end_frame(rr->gfx.state);
        return;
    }

    /* RTS_SCENE_ORBITAL_SHIPYARD: Dramatic 3/4 orbital showcase of Drydock, Dreadnought, Battlecruiser & Planet Limb */
    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.01f, 0.015f, 0.045f, 1.0f});
    RtsVec3f cam = {-76.0f, 38.0f, 102.0f};
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at(cam, (RtsVec3f){6.0f, 6.0f, -24.0f}, (RtsVec3f){0.0f, 1.0f, 0.0f}));

    draw_starfield(rr, vp, cam, time_s);

    /* Huge True-Scale Illuminated Planet Curved Limb Sweeping Below */
    draw_mesh(rr, rr->sh_mesh, rr->mesh_sphere, rr->tex_planet_surface, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 1.1), (RtsVec3f){-80.0f, -255.0f, -95.0f}, (RtsVec3f){235.0f, 235.0f, 235.0f}),
              (RtsVec4f){0.88f, 0.96f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(idq, (RtsVec3f){-80.0f, -255.0f, -95.0f}, (RtsVec3f){242.0f, 242.0f, 242.0f}),
              (RtsVec4f){0.18f, 0.76f, 1.0f, 0.65f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

    /* Colossal Orbital Shipyard Drydock (open U-frame facing camera-left) + Docked 680m Dreadnought 'Solaris' */
    RtsQuatd dock_rot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), -2.25);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_shipyard, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){34.0f, 14.0f, -34.0f}, (RtsVec3f){11.5f, 11.5f, 11.5f}),
              (RtsVec4f){0.90f, 0.94f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){26.0f, 10.0f, -28.0f}, (RtsVec3f){8.5f, 8.5f, 8.5f}),
              (RtsVec4f){1.0f, 0.98f, 0.92f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_shield, rr->mesh_sphere, RTS_GFX_INVALID, vp,
              rts_mat4_from_quat_trs(dock_rot, (RtsVec3f){26.0f, 10.0f, -28.0f}, (RtsVec3f){18.0f, 16.0f, 44.0f}),
              (RtsVec4f){0.22f, 0.90f, 1.0f, 0.52f}, light, time_s, true, true, false, false, RTS_DRAW_TRIANGLES);

    /* Crimson Dominion Battlecruiser engaging across Orbit + Escort Starfighters & Dropship */
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dreadnought, rr->tex_hull_red, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.72), (RtsVec3f){-56.0f, 22.0f, -54.0f}, (RtsVec3f){7.2f, 7.2f, 7.2f}),
              (RtsVec4f){1.0f, 0.82f, 0.78f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_starfighter, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), 0.6), (RtsVec3f){-14.0f, 22.0f, 16.0f}, (RtsVec3f){2.8f, 2.8f, 2.8f}),
              (RtsVec4f){1.0f, 1.0f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);
    draw_mesh(rr, rr->sh_mesh, rr->mesh_dropship, rr->tex_hull_blue, vp,
              rts_mat4_from_quat_trs(rts_quat_from_axis_angle(rts_v3d(0,1,0), -0.8), (RtsVec3f){-32.0f, 8.0f, 46.0f}, (RtsVec3f){2.8f, 2.8f, 2.8f}),
              (RtsVec4f){0.95f, 0.98f, 1.0f, 1.0f}, light, time_s, false, false, true, false, RTS_DRAW_TRIANGLES);

    /* Beam Lance & Plasma Fire across Orbit */
    draw_plasma_beam(rr, vp, (RtsVec3f){-34.0f, 22.0f, -38.0f}, (RtsVec3f){14.0f, 14.0f, -26.0f}, 1.15f, (RtsVec4f){1.0f, 0.22f, 0.18f, 1.0f}, time_s);
    draw_plasma_beam(rr, vp, (RtsVec3f){12.0f, 10.0f, -22.0f}, (RtsVec3f){-44.0f, 20.0f, -46.0f}, 0.95f, (RtsVec4f){0.22f, 0.94f, 1.0f, 1.0f}, time_s);
    draw_plasma_beam(rr, vp, (RtsVec3f){-14.0f, 22.0f, 14.0f}, (RtsVec3f){-48.0f, 22.0f, -44.0f}, 0.45f, (RtsVec4f){0.35f, 1.0f, 0.50f, 1.0f}, time_s);

    render_hud_common(rr, world, input,
                      "ORBITAL SHIPYARD ALPHA [400KM LEO] - DRYDOCK",
                      "VEL:7,672 M/S | ECC:0.002 | INCL:14.3 DEG | DREADNOUGHT 'SOLARIS'",
                      "ASTROGATOR: KEPLERIAN RENDEZVOUS COMPLETE. CRIMSON CRUISER OPENING BEAM LANCE FIRE!",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

void rts_renderer_render_live_world(RtsRetroRenderer *rr, const RtsWorld *world, const RtsInputRouter *input, float time_s) {
    if (!rr || !rr->initialized || !world) return;
    float aspect = (float)rr->width / (float)(rr->height > 0 ? rr->height : 1);
    RtsMat4f proj = rts_mat4_perspective((float)(62.0 * RTS_DEG2RAD), aspect, 0.2f, 2000.0f);
    RtsVec3f light = {0.55f, 0.80f, 0.35f};

    RtsVec3d cam_world = rts_v3d(0.0, 0.0, 0.0);
    rts_interior_local_to_world(&world->interiors, world->commander.frame_id, world->commander.local_pos, &cam_world);
    RtsVec3f fwd = {
        (float)(sin(world->commander.yaw_rad) * cos(world->commander.pitch_rad)),
        (float)(sin(world->commander.pitch_rad)),
        (float)(-cos(world->commander.yaw_rad) * cos(world->commander.pitch_rad))
    };
    RtsMat4f vp = rts_mat4_mul(proj, rts_mat4_look_at((RtsVec3f){0, 0, 0}, fwd, (RtsVec3f){0, 1, 0}));

    rr->gfx.begin_frame(rr->gfx.state, rr->width, rr->height, (RtsVec4f){0.03f, 0.08f, 0.18f, 1.0f});
    draw_starfield(rr, vp, (RtsVec3f){0, 0, 0}, time_s);

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
                      "WASD: WALK/PILOT | TAB: WRIST-PAD | E: INTERACT | 1-7: SHOWCASE SCENES",
                      "LIVE SIMULATION ACTIVE - FLOATING-ORIGIN 64-BIT CAMERA RELATIVE TO COMMANDER.",
                      false);
    rr->gfx.end_frame(rr->gfx.state);
}

bool rts_renderer_capture_png(RtsRetroRenderer *rr, const char *out_path) {
    if (!rr || !rr->initialized || !rr->gfx.capture_png) return false;
    return rr->gfx.capture_png(rr->gfx.state, out_path, rr->width, rr->height);
}
