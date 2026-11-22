/* SPDX-License-Identifier: MIT */
#include "rts/mod_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <dirent.h>
#endif

static void mm_extract_json(const char *json, const char *key, char *out, size_t cap) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *hit = strstr(json, pat);
    if (!hit) return;
    hit = strchr(hit + strlen(pat), ':');
    if (!hit) return;
    hit = strchr(hit, '"');
    if (!hit) return;
    ++hit;
    const char *end = strchr(hit, '"');
    if (!end) return;
    size_t n = (size_t)(end - hit);
    if (n >= cap) n = cap - 1u;
    memcpy(out, hit, n);
    out[n] = '\0';
}

static bool vfs_has_mod_enabled(const RtsVfs *vfs, const char *mod_id) {
    for (size_t i = 0; i < vfs->mod_count; ++i) {
        if (vfs->mods[i].enabled && strcmp(vfs->mods[i].id, mod_id) == 0) {
            return true;
        }
    }
    return false;
}

static void mm_try_add_folder(RtsModCatalog *cat, const RtsVfs *vfs, const char *folder) {
    if (!folder || folder[0] == '.' || cat->count >= RTS_VFS_MAX_MODS) return;
    if (!rts_vfs_validate_path(folder)) return;
    char manifest[RTS_VFS_MAX_PATH];
    int n = snprintf(manifest, sizeof(manifest), "%s/mods/%s/mod.json", vfs->base_dir, folder);
    if (n <= 0 || (size_t)n >= sizeof(manifest)) return;

    uint8_t *buf = NULL;
    size_t sz = 0u;
    if (!vfs->fs.read_all(vfs->fs.user_data, manifest, &buf, &sz) || !buf) {
        return;
    }

    RtsModCatalogEntry *e = &cat->entries[cat->count];
    memset(e, 0, sizeof(*e));
    snprintf(e->id, sizeof(e->id), "%.63s", folder);
    snprintf(e->name, sizeof(e->name), "%.95s", folder);
    snprintf(e->version, sizeof(e->version), "1.0.0");
    snprintf(e->description, sizeof(e->description), "External loose VFS mod.");
    mm_extract_json((const char *)buf, "id", e->id, sizeof(e->id));
    mm_extract_json((const char *)buf, "name", e->name, sizeof(e->name));
    mm_extract_json((const char *)buf, "version", e->version, sizeof(e->version));
    mm_extract_json((const char *)buf, "description", e->description, sizeof(e->description));
    e->enabled = vfs_has_mod_enabled(vfs, e->id);
    free(buf);
    cat->count++;
}

static int cmp_mod_entry(const void *a, const void *b) {
    const RtsModCatalogEntry *ea = (const RtsModCatalogEntry *)a;
    const RtsModCatalogEntry *eb = (const RtsModCatalogEntry *)b;
    return strcmp(ea->id, eb->id);
}

size_t rts_mod_manager_scan(RtsModCatalog *cat, const RtsVfs *vfs) {
    if (!cat || !vfs) return 0u;
    memset(cat, 0, sizeof(*cat));

    char mods_dir[RTS_VFS_MAX_PATH];
    int n = snprintf(mods_dir, sizeof(mods_dir), "%s/mods", vfs->base_dir);
    if (n <= 0 || (size_t)n >= sizeof(mods_dir)) return 0u;

#ifdef _WIN32
    char pattern[RTS_VFS_MAX_PATH + 4];
    snprintf(pattern, sizeof(pattern), "%s/*", mods_dir);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
                mm_try_add_folder(cat, vfs, ffd.cFileName);
            }
        } while (FindNextFileA(h, &ffd));
        FindClose(h);
    }
#else
    DIR *dir = opendir(mods_dir);
    if (dir) {
        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            mm_try_add_folder(cat, vfs, ent->d_name);
        }
        closedir(dir);
    }
#endif

    if (cat->count > 1u) {
        qsort(cat->entries, cat->count, sizeof(RtsModCatalogEntry), cmp_mod_entry);
    }
    return cat->count;
}

bool rts_mod_manager_toggle(RtsModCatalog *cat, RtsVfs *vfs, size_t index) {
    if (!cat || !vfs || index >= cat->count) return false;
    RtsModCatalogEntry *e = &cat->entries[index];
    if (e->enabled) {
        if (rts_vfs_unmount_mod(vfs, e->id)) {
            e->enabled = false;
            return true;
        }
        return false;
    }
    if (rts_vfs_mount_mod(vfs, e->id)) {
        e->enabled = true;
        return true;
    }
    return false;
}

bool rts_mod_manager_hot_reload(RtsVfs *vfs, RtsLuaVm *lua, RtsRetroRenderer *renderer) {
    if (!vfs) return false;
    bool ok = true;
    if (lua) {
        ok = rts_lua_exec_vfs_script(lua, "main.lua") && ok;
    }
    if (renderer && renderer->initialized) {
        ok = rts_renderer_reload_textures(renderer) && ok;
    }
    return ok;
}
