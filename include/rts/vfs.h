/* SPDX-License-Identifier: MIT */
#ifndef RTS_VFS_H
#define RTS_VFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_VFS_MAX_MODS 16
#define RTS_VFS_MAX_PATH 512

typedef struct RtsFsBackend {
    void *user_data;
    bool (*exists)(void *user_data, const char *os_path);
    bool (*read_all)(void *user_data, const char *os_path, uint8_t **out_buf, size_t *out_size);
    uint64_t (*mtime)(void *user_data, const char *os_path);
} RtsFsBackend;

typedef struct RtsModInfo {
    char id[64];
    char name[96];
    char version[32];
    char root_dir[RTS_VFS_MAX_PATH];
    bool enabled;
} RtsModInfo;

typedef struct RtsVfs {
    char base_dir[RTS_VFS_MAX_PATH];
    RtsFsBackend fs;
    RtsModInfo mods[RTS_VFS_MAX_MODS];
    size_t mod_count;
} RtsVfs;

RtsFsBackend rts_fs_os_backend(void);
bool rts_vfs_validate_path(const char *logical_path);
bool rts_vfs_init(RtsVfs *vfs, const char *base_dir, const RtsFsBackend *custom_fs);
bool rts_vfs_mount_mod(RtsVfs *vfs, const char *mod_folder_name);
bool rts_vfs_unmount_mod(RtsVfs *vfs, const char *mod_folder_name);
void rts_vfs_clear_mods(RtsVfs *vfs);
bool rts_vfs_resolve(const RtsVfs *vfs, const char *category, const char *logical_path, char *out_os_path, size_t out_cap);
bool rts_vfs_exists(const RtsVfs *vfs, const char *category, const char *logical_path);
bool rts_vfs_read_bytes(const RtsVfs *vfs, const char *category, const char *logical_path, uint8_t **out_data, size_t *out_size);
char *rts_vfs_read_text(const RtsVfs *vfs, const char *category, const char *logical_path, size_t *out_len);
uint64_t rts_vfs_mtime(const RtsVfs *vfs, const char *category, const char *logical_path);
void rts_vfs_free(void *ptr);

#define RTS_VFS_MAX_WATCHES 32

typedef struct RtsHotReloadEntry {
    char category[32];
    char logical_path[256];
    uint64_t last_mtime;
    bool active;
} RtsHotReloadEntry;

typedef struct RtsHotReloadWatch {
    RtsHotReloadEntry entries[RTS_VFS_MAX_WATCHES];
    size_t count;
} RtsHotReloadWatch;

void rts_vfs_watch_init(RtsHotReloadWatch *watch);
bool rts_vfs_watch_add(const RtsVfs *vfs, RtsHotReloadWatch *watch, const char *category, const char *logical_path);
size_t rts_vfs_check_hot_reload(const RtsVfs *vfs, RtsHotReloadWatch *watch, size_t *out_changed_indices, size_t max_out);

#endif /* RTS_VFS_H */
