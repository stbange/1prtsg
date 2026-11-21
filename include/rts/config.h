/* SPDX-License-Identifier: MIT */
#ifndef RTS_CONFIG_H
#define RTS_CONFIG_H

#include "rts/vfs.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>

#define RTS_CONFIG_MAX_MODS 8

typedef struct RtsEngineConfig {
    int window_width;
    int window_height;
    bool retro_nearest_textures;
    bool hardcore_ironman;
    double crew_sprite_lod_distance_m;
    double planet_radius_m;
    char enabled_mods[RTS_CONFIG_MAX_MODS][64];
    size_t enabled_mod_count;
} RtsEngineConfig;

void rts_config_defaults(RtsEngineConfig *cfg);
bool rts_config_parse_text(const char *text, RtsEngineConfig *cfg);
bool rts_config_load_vfs(const RtsVfs *vfs, const char *logical_path, RtsEngineConfig *cfg);
void rts_config_apply(const RtsEngineConfig *cfg, RtsVfs *vfs, RtsWorld *world);

#endif /* RTS_CONFIG_H */
