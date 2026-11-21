/* SPDX-License-Identifier: MIT */
#ifndef RTS_LUA_VM_H
#define RTS_LUA_VM_H

#include "rts/audio.h"
#include "rts/controller.h"
#include "rts/economy.h"
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct lua_State lua_State;

typedef struct RtsLuaCoverageFile {
    char name[160];
    uint32_t hit_mask[64]; /* Tracks lines 1..2048 per script */
    uint32_t max_line;
} RtsLuaCoverageFile;

#define RTS_LUA_COV_MAX_FILES 64

typedef struct RtsLuaVm {
    lua_State *L;
    RtsVfs *vfs;
    RtsWorld *world;
    RtsInputRouter *input;
    RtsVoxelChunk *voxel;
    RtsRetroRenderer *renderer;
    RtsAudioEngine *audio;
    RtsEconomySystem *economy;
    RtsCommanderController *controller;
    char last_error[512];

    bool coverage_enabled;
    RtsLuaCoverageFile cov_files[RTS_LUA_COV_MAX_FILES];
    size_t cov_file_count;
} RtsLuaVm;

bool rts_lua_init(RtsLuaVm *vm, RtsVfs *vfs, RtsWorld *world, RtsInputRouter *input, RtsVoxelChunk *voxel, RtsRetroRenderer *renderer);
void rts_lua_attach_phase2(RtsLuaVm *vm, RtsAudioEngine *audio, RtsEconomySystem *economy, RtsCommanderController *controller);
void rts_lua_shutdown(RtsLuaVm *vm);
void rts_lua_enable_coverage(RtsLuaVm *vm, bool enable);
bool rts_lua_exec_vfs_script(RtsLuaVm *vm, const char *script_path);
bool rts_lua_exec_string(RtsLuaVm *vm, const char *code, const char *chunk_name);
double rts_lua_report_coverage(const RtsLuaVm *vm, double min_percent, bool verbose);

#endif /* RTS_LUA_VM_H */
