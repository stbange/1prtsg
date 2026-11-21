/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/lua_vm.h"
#include "rts/platform.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int run_headless(const RtsAppConfig *cfg) {
    RtsVfs vfs;
    if (!rts_vfs_init(&vfs, cfg->data_dir ? cfg->data_dir : ".", NULL)) {
        fprintf(stderr, "[1prtsg] VFS init failed\n");
        return 1;
    }
    if (cfg->mod_to_mount && cfg->mod_to_mount[0] != '\0') {
        if (!rts_vfs_mount_mod(&vfs, cfg->mod_to_mount)) {
            fprintf(stderr, "[1prtsg] Failed to mount mod '%s'\n", cfg->mod_to_mount);
            return 1;
        }
    }

    RtsWorld world;
    rts_world_init(&world, rts_planet_earthlike(42u), 1337u);
    RtsInputRouter input;
    rts_input_init(&input, (float)cfg->width, (float)cfg->height, false);
    RtsVoxelChunk voxel;
    rts_voxel_init(&voxel, 1.0f);
    RtsRetroRenderer renderer;
    rts_renderer_init(&renderer, rts_gfx_null_create(), &vfs, cfg->width, cfg->height);

    RtsLuaVm lua;
    if (!rts_lua_init(&lua, &vfs, &world, &input, &voxel, &renderer)) {
        fprintf(stderr, "[1prtsg] Lua VM init failed\n");
        rts_renderer_shutdown(&renderer);
        return 1;
    }
    if (cfg->lua_coverage) {
        rts_lua_enable_coverage(&lua, true);
    }

    const char *script = cfg->test_script ? cfg->test_script : "main.lua";
    int rc = 0;
    if (!rts_lua_exec_vfs_script(&lua, script)) {
        fprintf(stderr, "[1prtsg] Lua error in '%s': %s\n", script, lua.last_error);
        rc = 1;
    }

    if (rc == 0 && cfg->lua_coverage) {
        double pct = rts_lua_report_coverage(&lua, cfg->min_lua_coverage, true);
        if (pct < cfg->min_lua_coverage) {
            fprintf(stderr, "[1prtsg] FAIL: Lua coverage %.2f%% < %.2f%%\n", pct, cfg->min_lua_coverage);
            rc = 1;
        } else {
            printf("[1prtsg] PASS: Lua coverage %.2f%% >= %.2f%%\n", pct, cfg->min_lua_coverage);
        }
    }

    rts_lua_shutdown(&lua);
    rts_renderer_shutdown(&renderer);
    return rc;
}

int main(int argc, char **argv) {
    RtsAppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.data_dir = ".";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.min_lua_coverage = 95.0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0) {
            cfg.headless = true;
        } else if (strcmp(argv[i], "--lua-coverage") == 0) {
            cfg.lua_coverage = true;
        } else if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
            cfg.data_dir = argv[++i];
        } else if (strcmp(argv[i], "--test-script") == 0 && i + 1 < argc) {
            cfg.test_script = argv[++i];
        } else if (strcmp(argv[i], "--mod") == 0 && i + 1 < argc) {
            cfg.mod_to_mount = argv[++i];
        } else if (strcmp(argv[i], "--capture-screenshots") == 0 && i + 1 < argc) {
            cfg.screenshot_dir = argv[++i];
        } else if (strcmp(argv[i], "--min-lua-coverage") == 0 && i + 1 < argc) {
            cfg.min_lua_coverage = atof(argv[++i]);
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg.width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg.height = atoi(argv[++i]);
        }
    }

    if (cfg.headless) {
        return run_headless(&cfg);
    }
    if (cfg.screenshot_dir) {
        return rts_platform_capture_screenshots(&cfg);
    }
    return rts_platform_run_interactive(&cfg);
}
