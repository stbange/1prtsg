/* SPDX-License-Identifier: MIT */
#include "rts/fun_eval.h"
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

typedef struct HeadlessState {
    RtsVfs vfs;
    RtsWorld world;
    RtsInputRouter input;
    RtsVoxelChunk voxel;
    RtsAudioEngine audio;
    RtsEconomySystem economy;
    RtsCommanderController ctrl;
    RtsBoardingSystem boarding;
    RtsRetroRenderer renderer;
    RtsLuaVm lua;
} HeadlessState;

static int run_headless(const RtsAppConfig *cfg) {
    HeadlessState *hs = (HeadlessState *)calloc(1, sizeof(HeadlessState));
    if (!hs) {
        fprintf(stderr, "[1prtsg] Out of memory\n");
        return 1;
    }
    if (!rts_vfs_init(&hs->vfs, cfg->data_dir ? cfg->data_dir : ".", NULL)) {
        fprintf(stderr, "[1prtsg] VFS init failed\n");
        free(hs);
        return 1;
    }
    if (cfg->mod_to_mount && cfg->mod_to_mount[0] != '\0') {
        if (!rts_vfs_mount_mod(&hs->vfs, cfg->mod_to_mount)) {
            fprintf(stderr, "[1prtsg] Failed to mount mod '%s'\n", cfg->mod_to_mount);
            free(hs);
            return 1;
        }
    }

    rts_world_init(&hs->world, rts_planet_earthlike(42u), cfg->seed != 0u ? cfg->seed : 1337u);
    rts_input_init(&hs->input, (float)cfg->width, (float)cfg->height, false);
    rts_voxel_init(&hs->voxel, 1.0f);
    rts_audio_init(&hs->audio, rts_audio_null_create(), &hs->vfs, 22050);
    rts_economy_init(&hs->economy);
    rts_controller_init(&hs->ctrl);
    rts_boarding_init(&hs->boarding);
    rts_renderer_init(&hs->renderer, rts_gfx_null_create(), &hs->vfs, cfg->width, cfg->height);

    if (!rts_lua_init(&hs->lua, &hs->vfs, &hs->world, &hs->input, &hs->voxel, &hs->renderer)) {
        fprintf(stderr, "[1prtsg] Lua VM init failed\n");
        rts_audio_shutdown(&hs->audio);
        rts_renderer_shutdown(&hs->renderer);
        free(hs);
        return 1;
    }
    rts_lua_attach_phase2(&hs->lua, &hs->audio, &hs->economy, &hs->ctrl);
    rts_lua_attach_phase3(&hs->lua, &hs->boarding);
    if (cfg->lua_coverage) {
        rts_lua_enable_coverage(&hs->lua, true);
    }

    const char *script = cfg->test_script ? cfg->test_script : "main.lua";
    int rc = 0;
    if (!rts_lua_exec_vfs_script(&hs->lua, script)) {
        fprintf(stderr, "[1prtsg] Lua error in '%s': %s\n", script, hs->lua.last_error);
        rc = 1;
    }

    if (rc == 0 && cfg->lua_coverage) {
        double pct = rts_lua_report_coverage(&hs->lua, cfg->min_lua_coverage, true);
        if (pct < cfg->min_lua_coverage) {
            fprintf(stderr, "[1prtsg] FAIL: Lua coverage %.2f%% < %.2f%%\n", pct, cfg->min_lua_coverage);
            rc = 1;
        } else {
            printf("[1prtsg] PASS: Lua coverage %.2f%% >= %.2f%%\n", pct, cfg->min_lua_coverage);
        }
    }

    rts_lua_shutdown(&hs->lua);
    rts_audio_shutdown(&hs->audio);
    rts_renderer_shutdown(&hs->renderer);
    free(hs);
    return rc;
}

static int run_fun_eval(const RtsAppConfig *cfg) {
    RtsFeelConfig feel = cfg->fun_baseline ? rts_feel_config_baseline() : rts_feel_config_solaris();
    int episodes = cfg->fun_episodes > 0 ? cfg->fun_episodes : 25;
    uint64_t seed = cfg->seed != 0u ? cfg->seed : 42u;
    RtsFunAuditReport rep;
    if (!rts_fun_run_audit(&feel, seed, episodes, cfg->telemetry_out, 0.0, &rep)) {
        fprintf(stderr, "[fun_eval] Audit failed\n");
        return 1;
    }
    rts_fun_print_report(&rep, stdout);
    return 0;
}

int main(int argc, char **argv) {
    RtsAppConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    cfg.data_dir = ".";
    cfg.width = 1280;
    cfg.height = 720;
    cfg.seed = 42u;
    cfg.fun_episodes = 25;
    cfg.min_lua_coverage = 95.0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--headless") == 0) {
            cfg.headless = true;
        } else if (strcmp(argv[i], "--lua-coverage") == 0) {
            cfg.lua_coverage = true;
        } else if (strcmp(argv[i], "--fun-eval") == 0) {
            cfg.fun_eval = true;
        } else if (strcmp(argv[i], "--baseline") == 0) {
            cfg.fun_baseline = true;
        } else if (strcmp(argv[i], "--seed") == 0 && i + 1 < argc) {
            cfg.seed = (uint64_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--episodes") == 0 && i + 1 < argc) {
            cfg.fun_episodes = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--telemetry-out") == 0 && i + 1 < argc) {
            cfg.telemetry_out = argv[++i];
        } else if (strcmp(argv[i], "--data-dir") == 0 && i + 1 < argc) {
            cfg.data_dir = argv[++i];
        } else if (strcmp(argv[i], "--test-script") == 0 && i + 1 < argc) {
            cfg.test_script = argv[++i];
        } else if (strcmp(argv[i], "--mod") == 0 && i + 1 < argc) {
            cfg.mod_to_mount = argv[++i];
        } else if (strcmp(argv[i], "--capture-screenshots") == 0 && i + 1 < argc) {
            cfg.screenshot_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-roster-gallery") == 0 && i + 1 < argc) {
            cfg.roster_gallery_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-mega-interiors") == 0 && i + 1 < argc) {
            cfg.mega_interiors_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-rts-toolkit") == 0 && i + 1 < argc) {
            cfg.rts_toolkit_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-milestone-011") == 0 && i + 1 < argc) {
            cfg.milestone_011_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-milestone-012") == 0 && i + 1 < argc) {
            cfg.milestone_012_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-milestone-013") == 0 && i + 1 < argc) {
            cfg.milestone_013_dir = argv[++i];
        } else if (strcmp(argv[i], "--capture-fun-frames") == 0 && i + 1 < argc) {
            cfg.fun_frames_dir = argv[++i];
        } else if (strcmp(argv[i], "--min-lua-coverage") == 0 && i + 1 < argc) {
            cfg.min_lua_coverage = atof(argv[++i]);
        } else if (strcmp(argv[i], "--width") == 0 && i + 1 < argc) {
            cfg.width = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--height") == 0 && i + 1 < argc) {
            cfg.height = atoi(argv[++i]);
        }
    }

    if (cfg.fun_eval) {
        return run_fun_eval(&cfg);
    }
    if (cfg.headless) {
        return run_headless(&cfg);
    }
    if (cfg.milestone_013_dir) {
        return rts_platform_capture_milestone_013(&cfg);
    }
    if (cfg.milestone_012_dir) {
        return rts_platform_capture_milestone_012(&cfg);
    }
    if (cfg.milestone_011_dir) {
        return rts_platform_capture_milestone_011(&cfg);
    }
    if (cfg.rts_toolkit_dir) {
        return rts_platform_capture_rts_toolkit(&cfg);
    }
    if (cfg.mega_interiors_dir) {
        return rts_platform_capture_mega_interiors(&cfg);
    }
    if (cfg.roster_gallery_dir) {
        return rts_platform_capture_roster_gallery(&cfg);
    }
    if (cfg.fun_frames_dir) {
        return rts_platform_capture_fun_frames(&cfg);
    }
    if (cfg.screenshot_dir) {
        return rts_platform_capture_screenshots(&cfg);
    }
    return rts_platform_run_interactive(&cfg);
}
