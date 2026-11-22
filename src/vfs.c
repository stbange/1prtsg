/* SPDX-License-Identifier: MIT */
#include "rts/vfs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static bool os_fs_exists(void *user_data, const char *os_path) {
    (void)user_data;
    struct stat st;
    return stat(os_path, &st) == 0;
}

static bool os_fs_read_all(void *user_data, const char *os_path, uint8_t **out_buf, size_t *out_size) {
    (void)user_data;
    FILE *fp = fopen(os_path, "rb");
    if (!fp) {
        return false;
    }
    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        return false;
    }
    long len = ftell(fp);
    if (len < 0) {
        fclose(fp);
        return false;
    }
    rewind(fp);
    uint8_t *buf = (uint8_t *)malloc((size_t)len + 1u);
    if (!buf) {
        fclose(fp);
        return false;
    }
    size_t read_n = (len > 0) ? fread(buf, 1, (size_t)len, fp) : 0u;
    fclose(fp);
    if (read_n != (size_t)len) {
        free(buf);
        return false;
    }
    buf[len] = 0;
    *out_buf = buf;
    if (out_size) {
        *out_size = (size_t)len;
    }
    return true;
}

static uint64_t os_fs_mtime(void *user_data, const char *os_path) {
    (void)user_data;
    struct stat st;
    if (stat(os_path, &st) != 0) {
        return 0u;
    }
    return (uint64_t)st.st_mtime;
}

RtsFsBackend rts_fs_os_backend(void) {
    RtsFsBackend b;
    b.user_data = NULL;
    b.exists = os_fs_exists;
    b.read_all = os_fs_read_all;
    b.mtime = os_fs_mtime;
    return b;
}

bool rts_vfs_validate_path(const char *logical_path) {
    if (!logical_path || logical_path[0] == '\0') {
        return false;
    }
    if (logical_path[0] == '/' || logical_path[0] == '\\') {
        return false;
    }
    if (strchr(logical_path, ':') != NULL) {
        return false;
    }
    const char *p = logical_path;
    while (*p != '\0') {
        if (*p == '\\') {
            return false;
        }
        if (p[0] == '.' && p[1] == '.' &&
            (p == logical_path || p[-1] == '/') &&
            (p[2] == '\0' || p[2] == '/')) {
            return false;
        }
        ++p;
    }
    return true;
}

bool rts_vfs_init(RtsVfs *vfs, const char *base_dir, const RtsFsBackend *custom_fs) {
    if (!vfs || !base_dir || base_dir[0] == '\0') {
        return false;
    }
    memset(vfs, 0, sizeof(*vfs));
    size_t len = strlen(base_dir);
    if (len >= sizeof(vfs->base_dir)) {
        return false;
    }
    memcpy(vfs->base_dir, base_dir, len + 1);
    while (len > 1 && (vfs->base_dir[len - 1] == '/' || vfs->base_dir[len - 1] == '\\')) {
        vfs->base_dir[len - 1] = '\0';
        --len;
    }
    vfs->fs = custom_fs ? *custom_fs : rts_fs_os_backend();
    return true;
}

static void extract_json_string(const char *json, const char *key, char *out, size_t cap) {
    char pat[96];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *hit = strstr(json, pat);
    if (!hit) {
        return;
    }
    hit = strchr(hit + strlen(pat), ':');
    if (!hit) {
        return;
    }
    hit = strchr(hit, '"');
    if (!hit) {
        return;
    }
    ++hit;
    const char *end = strchr(hit, '"');
    if (!end) {
        return;
    }
    size_t n = (size_t)(end - hit);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, hit, n);
    out[n] = '\0';
}

bool rts_vfs_mount_mod(RtsVfs *vfs, const char *mod_folder_name) {
    if (!vfs || !rts_vfs_validate_path(mod_folder_name) || vfs->mod_count >= RTS_VFS_MAX_MODS) {
        return false;
    }
    for (size_t i = 0; i < vfs->mod_count; ++i) {
        if (strcmp(vfs->mods[i].id, mod_folder_name) == 0) {
            vfs->mods[i].enabled = true;
            return true;
        }
    }
    char manifest_path[RTS_VFS_MAX_PATH];
    int written = snprintf(manifest_path, sizeof(manifest_path), "%s/mods/%s/mod.json", vfs->base_dir, mod_folder_name);
    if (written <= 0 || (size_t)written >= sizeof(manifest_path)) {
        return false;
    }
    uint8_t *buf = NULL;
    size_t sz = 0;
    if (!vfs->fs.read_all(vfs->fs.user_data, manifest_path, &buf, &sz)) {
        return false;
    }

    RtsModInfo *m = &vfs->mods[vfs->mod_count];
    memset(m, 0, sizeof(*m));
    strncpy(m->id, mod_folder_name, sizeof(m->id) - 1);
    strncpy(m->name, mod_folder_name, sizeof(m->name) - 1);
    strncpy(m->version, "1.0.0", sizeof(m->version) - 1);
    char root_tmp[RTS_VFS_MAX_PATH * 2];
    int rn = snprintf(root_tmp, sizeof(root_tmp), "%s/mods/%s", vfs->base_dir, mod_folder_name);
    if (rn <= 0 || (size_t)rn >= sizeof(m->root_dir)) {
        free(buf);
        return false;
    }
    memcpy(m->root_dir, root_tmp, (size_t)rn + 1);
    extract_json_string((const char *)buf, "id", m->id, sizeof(m->id));
    extract_json_string((const char *)buf, "name", m->name, sizeof(m->name));
    extract_json_string((const char *)buf, "version", m->version, sizeof(m->version));
    m->enabled = true;
    free(buf);
    vfs->mod_count++;
    return true;
}

bool rts_vfs_unmount_mod(RtsVfs *vfs, const char *mod_folder_name) {
    if (!vfs || !mod_folder_name) return false;
    for (size_t i = 0; i < vfs->mod_count; ++i) {
        if (strcmp(vfs->mods[i].id, mod_folder_name) == 0) {
            for (size_t j = i + 1; j < vfs->mod_count; ++j) {
                vfs->mods[j - 1] = vfs->mods[j];
            }
            vfs->mod_count--;
            memset(&vfs->mods[vfs->mod_count], 0, sizeof(RtsModInfo));
            return true;
        }
    }
    return false;
}

void rts_vfs_clear_mods(RtsVfs *vfs) {
    if (!vfs) return;
    memset(vfs->mods, 0, sizeof(vfs->mods));
    vfs->mod_count = 0u;
}

bool rts_vfs_resolve(const RtsVfs *vfs, const char *category, const char *logical_path, char *out_os_path, size_t out_cap) {
    if (!vfs || !category || !rts_vfs_validate_path(category) || !rts_vfs_validate_path(logical_path) || !out_os_path || out_cap == 0) {
        return false;
    }
    char cand[RTS_VFS_MAX_PATH * 2];
    /* Reverse mod load order: later mounted mods take highest priority */
    for (size_t i = vfs->mod_count; i > 0; --i) {
        const RtsModInfo *m = &vfs->mods[i - 1];
        if (!m->enabled) {
            continue;
        }
        int n = snprintf(cand, sizeof(cand), "%s/%s/%s", m->root_dir, category, logical_path);
        if (n > 0 && (size_t)n < sizeof(cand) && vfs->fs.exists(vfs->fs.user_data, cand)) {
            if ((size_t)n >= out_cap) {
                return false;
            }
            memcpy(out_os_path, cand, (size_t)n + 1);
            return true;
        }
    }
    int n = snprintf(cand, sizeof(cand), "%s/%s/%s", vfs->base_dir, category, logical_path);
    if (n > 0 && (size_t)n < sizeof(cand) && vfs->fs.exists(vfs->fs.user_data, cand)) {
        if ((size_t)n >= out_cap) {
            return false;
        }
        memcpy(out_os_path, cand, (size_t)n + 1);
        return true;
    }
    return false;
}

bool rts_vfs_exists(const RtsVfs *vfs, const char *category, const char *logical_path) {
    char resolved[RTS_VFS_MAX_PATH];
    return rts_vfs_resolve(vfs, category, logical_path, resolved, sizeof(resolved));
}

bool rts_vfs_read_bytes(const RtsVfs *vfs, const char *category, const char *logical_path, uint8_t **out_data, size_t *out_size) {
    char resolved[RTS_VFS_MAX_PATH];
    if (!out_data || !rts_vfs_resolve(vfs, category, logical_path, resolved, sizeof(resolved))) {
        return false;
    }
    return vfs->fs.read_all(vfs->fs.user_data, resolved, out_data, out_size);
}

char *rts_vfs_read_text(const RtsVfs *vfs, const char *category, const char *logical_path, size_t *out_len) {
    uint8_t *bytes = NULL;
    size_t sz = 0;
    if (!rts_vfs_read_bytes(vfs, category, logical_path, &bytes, &sz)) {
        return NULL;
    }
    if (out_len) {
        *out_len = sz;
    }
    return (char *)bytes;
}

uint64_t rts_vfs_mtime(const RtsVfs *vfs, const char *category, const char *logical_path) {
    char resolved[RTS_VFS_MAX_PATH];
    if (!rts_vfs_resolve(vfs, category, logical_path, resolved, sizeof(resolved))) {
        return 0u;
    }
    return vfs->fs.mtime(vfs->fs.user_data, resolved);
}

void rts_vfs_free(void *ptr) {
    free(ptr);
}

void rts_vfs_watch_init(RtsHotReloadWatch *watch) {
    if (!watch) return;
    memset(watch, 0, sizeof(*watch));
}

bool rts_vfs_watch_add(const RtsVfs *vfs, RtsHotReloadWatch *watch, const char *category, const char *logical_path) {
    if (!vfs || !watch || !category || !logical_path || watch->count >= RTS_VFS_MAX_WATCHES) {
        return false;
    }
    if (!rts_vfs_exists(vfs, category, logical_path)) {
        return false;
    }
    RtsHotReloadEntry *e = &watch->entries[watch->count++];
    memset(e, 0, sizeof(*e));
    strncpy(e->category, category, sizeof(e->category) - 1);
    strncpy(e->logical_path, logical_path, sizeof(e->logical_path) - 1);
    e->last_mtime = rts_vfs_mtime(vfs, category, logical_path);
    e->active = true;
    return true;
}

size_t rts_vfs_check_hot_reload(const RtsVfs *vfs, RtsHotReloadWatch *watch, size_t *out_changed_indices, size_t max_out) {
    if (!vfs || !watch) return 0u;
    size_t changed = 0u;
    for (size_t i = 0; i < watch->count; ++i) {
        RtsHotReloadEntry *e = &watch->entries[i];
        if (!e->active) continue;
        uint64_t cur = rts_vfs_mtime(vfs, e->category, e->logical_path);
        if (cur != 0u && cur != e->last_mtime) {
            e->last_mtime = cur;
            if (out_changed_indices && changed < max_out) {
                out_changed_indices[changed] = i;
            }
            changed++;
        }
    }
    return changed;
}
