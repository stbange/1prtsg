/* SPDX-License-Identifier: MIT */
#ifndef RTS_PLANET_MESH_H
#define RTS_PLANET_MESH_H

#include "rts/gfx.h"
#include "rts/planet.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_PLANET_PATCH_GRID 18

typedef struct RtsPlanetLodSpec {
    int lod_level;        /* 0 = boots-on-ground 1m..360m, 4 = high orbit 1200km */
    double half_span_m;
    int grid_res;
    bool render_ocean;
    bool render_clouds;
    float atmo_blend;     /* 0 = surface haze, 1 = orbital limb glow */
} RtsPlanetLodSpec;

/* Select adaptive multi-LOD patch parameters from Commander altitude above sea level. */
RtsPlanetLodSpec rts_planet_select_lod(double altitude_m);

/* Blended tactical-to-global elevation relative to mean radius R (keeps base plateau dry and coastline accessible). */
double rts_planet_local_elevation(const RtsPlanetConfig *cfg, RtsVec3d unit_dir);

/* True when a unit normal lies in ocean waters (elevation < sea_level_m). */
bool rts_planet_local_is_ocean(const RtsPlanetConfig *cfg, RtsVec3d unit_dir);

/* Compute smooth surface normal at world direction via finite differences on the sphere. */
RtsVec3d rts_planet_local_normal(const RtsPlanetConfig *cfg, RtsVec3d unit_dir, double sample_step_m);

/* Build camera-relative spherical terrain triangles around cam_world (zero float32 jitter!). */
size_t rts_planet_build_surface_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                      RtsPlanetLodSpec lod, RtsVertex *out_verts, size_t max_verts);

/* Build animated specular spherical ocean wave mesh around cam_world. */
size_t rts_planet_build_ocean_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                    RtsPlanetLodSpec lod, float time_s,
                                    RtsVertex *out_verts, size_t max_verts);

/* Build translucent high-altitude cloud ring vertices for orbital/high-altitude flight. */
size_t rts_planet_build_cloud_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                    float time_s, RtsVertex *out_verts, size_t max_verts);

#endif /* RTS_PLANET_MESH_H */
