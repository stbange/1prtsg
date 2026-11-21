/* SPDX-License-Identifier: MIT */
#include "rts/voxel.h"
#include <stdio.h>
#include <string.h>

static int voxel_idx(int x, int y, int z) {
    return (y * RTS_VOXEL_DIM + z) * RTS_VOXEL_DIM + x;
}

void rts_voxel_init(RtsVoxelChunk *chunk, float voxel_size_m) {
    memset(chunk, 0, sizeof(*chunk));
    chunk->voxel_size_m = voxel_size_m > 0.01f ? voxel_size_m : 1.0f;
    for (int i = 0; i < 256; ++i) {
        chunk->palette[i].x = 0.65f;
        chunk->palette[i].y = 0.70f;
        chunk->palette[i].z = 0.78f;
    }
    chunk->palette[1] = (RtsVec3f){0.20f, 0.45f, 0.90f}; /* Solar Directorate Blue */
    chunk->palette[2] = (RtsVec3f){0.92f, 0.76f, 0.20f}; /* Gold trim */
    chunk->palette[3] = (RtsVec3f){0.85f, 0.18f, 0.22f}; /* Crimson Dominion Red */
    chunk->palette[4] = (RtsVec3f){0.25f, 0.88f, 0.95f}; /* Plasma / Cyan glow */
    chunk->dirty = true;
}

void rts_voxel_set_palette(RtsVoxelChunk *chunk, uint8_t index, RtsVec3f rgb) {
    if (!chunk) return;
    chunk->palette[index] = rgb;
    chunk->dirty = true;
}

bool rts_voxel_set(RtsVoxelChunk *chunk, int x, int y, int z, uint8_t value) {
    if (!chunk || x < 0 || x >= RTS_VOXEL_DIM || y < 0 || y >= RTS_VOXEL_DIM || z < 0 || z >= RTS_VOXEL_DIM) {
        return false;
    }
    chunk->cells[voxel_idx(x, y, z)] = value;
    chunk->dirty = true;
    return true;
}

uint8_t rts_voxel_get(const RtsVoxelChunk *chunk, int x, int y, int z) {
    if (!chunk || x < 0 || x >= RTS_VOXEL_DIM || y < 0 || y >= RTS_VOXEL_DIM || z < 0 || z >= RTS_VOXEL_DIM) {
        return 0u;
    }
    return chunk->cells[voxel_idx(x, y, z)];
}

void rts_voxel_fill_box(RtsVoxelChunk *chunk, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t value) {
    if (!chunk) return;
    for (int y = y0; y <= y1; ++y) {
        for (int z = z0; z <= z1; ++z) {
            for (int x = x0; x <= x1; ++x) {
                rts_voxel_set(chunk, x, y, z, value);
            }
        }
    }
}

static size_t emit_face(RtsVoxelVertex *out, size_t count, size_t max_verts, const float p[4][3], float nx, float ny, float nz, RtsVec3f col) {
    if (count + 6 > max_verts) {
        return count;
    }
    static const int tri[6] = {0, 1, 2, 0, 2, 3};
    static const float uv[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
    for (int i = 0; i < 6; ++i) {
        int k = tri[i];
        RtsVoxelVertex v = {p[k][0], p[k][1], p[k][2], nx, ny, nz, uv[k][0], uv[k][1], col.x, col.y, col.z};
        out[count++] = v;
    }
    return count;
}

size_t rts_voxel_build_mesh(const RtsVoxelChunk *chunk, RtsVoxelVertex *out_verts, size_t max_verts) {
    if (!chunk || !out_verts || max_verts < 6) {
        return 0u;
    }
    size_t count = 0;
    float s = chunk->voxel_size_m;
    for (int y = 0; y < RTS_VOXEL_DIM; ++y) {
        for (int z = 0; z < RTS_VOXEL_DIM; ++z) {
            for (int x = 0; x < RTS_VOXEL_DIM; ++x) {
                uint8_t val = rts_voxel_get(chunk, x, y, z);
                if (val == 0u) {
                    continue;
                }
                RtsVec3f col = chunk->palette[val];
                float x0 = (float)x * s, x1 = (float)(x + 1) * s;
                float y0 = (float)y * s, y1 = (float)(y + 1) * s;
                float z0 = (float)z * s, z1 = (float)(z + 1) * s;

                if (rts_voxel_get(chunk, x + 1, y, z) == 0u) {
                    const float f[4][3] = {{x1, y0, z1}, {x1, y0, z0}, {x1, y1, z0}, {x1, y1, z1}};
                    count = emit_face(out_verts, count, max_verts, f, 1, 0, 0, col);
                }
                if (rts_voxel_get(chunk, x - 1, y, z) == 0u) {
                    const float f[4][3] = {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z1}, {x0, y1, z0}};
                    count = emit_face(out_verts, count, max_verts, f, -1, 0, 0, col);
                }
                if (rts_voxel_get(chunk, x, y + 1, z) == 0u) {
                    const float f[4][3] = {{x0, y1, z1}, {x1, y1, z1}, {x1, y1, z0}, {x0, y1, z0}};
                    count = emit_face(out_verts, count, max_verts, f, 0, 1, 0, col);
                }
                if (rts_voxel_get(chunk, x, y - 1, z) == 0u) {
                    const float f[4][3] = {{x0, y0, z0}, {x1, y0, z0}, {x1, y0, z1}, {x0, y0, z1}};
                    count = emit_face(out_verts, count, max_verts, f, 0, -1, 0, col);
                }
                if (rts_voxel_get(chunk, x, y, z + 1) == 0u) {
                    const float f[4][3] = {{x0, y0, z1}, {x1, y0, z1}, {x1, y1, z1}, {x0, y1, z1}};
                    count = emit_face(out_verts, count, max_verts, f, 0, 0, 1, col);
                }
                if (rts_voxel_get(chunk, x, y, z - 1) == 0u) {
                    const float f[4][3] = {{x1, y0, z0}, {x0, y0, z0}, {x0, y1, z0}, {x1, y1, z0}};
                    count = emit_face(out_verts, count, max_verts, f, 0, 0, -1, col);
                }
            }
        }
    }
    return count;
}

bool rts_voxel_load_text(RtsVoxelChunk *chunk, const char *text_data) {
    if (!chunk || !text_data) {
        return false;
    }
    rts_voxel_init(chunk, chunk->voxel_size_m);
    const char *p = text_data;
    int loaded = 0;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') ++p;
        if (*p == '\0') break;
        if (*p == '#') {
            while (*p && *p != '\n') ++p;
            continue;
        }
        int x = 0, y = 0, z = 0, v = 0;
        if (sscanf(p, "%d %d %d %d", &x, &y, &z, &v) == 4) {
            if (rts_voxel_set(chunk, x, y, z, (uint8_t)v)) {
                loaded++;
            }
        }
        while (*p && *p != '\n') ++p;
    }
    return loaded > 0;
}
