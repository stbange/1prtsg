/* SPDX-License-Identifier: MIT */
#ifndef RTS_MOD_MANAGER_H
#define RTS_MOD_MANAGER_H

#include "rts/gfx.h"
#include "rts/lua_vm.h"
#include "rts/vfs.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct RtsModCatalogEntry {
    char id[64];
    char name[96];
    char version[32];
    char description[160];
    bool enabled;
} RtsModCatalogEntry;

typedef struct RtsModCatalog {
    RtsModCatalogEntry entries[RTS_VFS_MAX_MODS];
    size_t count;
} RtsModCatalog;

/* Scan <base_dir>/mods/ for external mod.json manifests (POSIX, Win32, Emscripten MEMFS) */
size_t rts_mod_manager_scan(RtsModCatalog *cat, const RtsVfs *vfs);

/* Enable or disable a discovered mod on the active VFS */
bool rts_mod_manager_toggle(RtsModCatalog *cat, RtsVfs *vfs, size_t index);

/* Re-execute main.lua via VFS and re-upload overridden external renderer textures live */
bool rts_mod_manager_hot_reload(RtsVfs *vfs, RtsLuaVm *lua, RtsRetroRenderer *renderer);

#endif /* RTS_MOD_MANAGER_H */
