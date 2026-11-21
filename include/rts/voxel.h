/* SPDX-License-Identifier: MIT */
#ifndef RTS_VOXEL_H
#define RTS_VOXEL_H

#include "rts/math3d.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_VOXEL_DIM 16
#define RTS_VOXEL_COUNT (RTS_VOXEL_DIM * RTS_VOXEL_DIM * RTS_VOXEL_DIM)

typedef struct RtsVoxelVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float r, g, b;
} RtsVoxelVertex;

typedef struct RtsVoxelChunk {
    uint8_t cells[RTS_VOXEL_COUNT];
    RtsVec3f palette[256];
    float voxel_size_m;
    bool dirty;
} RtsVoxelChunk;

void rts_voxel_init(RtsVoxelChunk *chunk, float voxel_size_m);
void rts_voxel_set_palette(RtsVoxelChunk *chunk, uint8_t index, RtsVec3f rgb);
bool rts_voxel_set(RtsVoxelChunk *chunk, int x, int y, int z, uint8_t value);
uint8_t rts_voxel_get(const RtsVoxelChunk *chunk, int x, int y, int z);
void rts_voxel_fill_box(RtsVoxelChunk *chunk, int x0, int y0, int z0, int x1, int y1, int z1, uint8_t value);
size_t rts_voxel_build_mesh(const RtsVoxelChunk *chunk, RtsVoxelVertex *out_verts, size_t max_verts);
bool rts_voxel_load_text(RtsVoxelChunk *chunk, const char *text_data);

#endif /* RTS_VOXEL_H */
