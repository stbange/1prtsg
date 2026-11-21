/* SPDX-License-Identifier: MIT */
#include "rts/fun_eval.h"
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/lua_vm.h"
#include "rts/platform.h"
#include "rts/save_load.h"
#include "rts/tactical_sim.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef enum RtsLiveViewMode {
    RTS_VIEW_SHOWCASE = 0,
    RTS_VIEW_TACTICAL = 1,
    RTS_VIEW_SANDBOX_RTS = 2
} RtsLiveViewMode;

typedef struct InteractiveApp {
    SDL_Window *window;
    SDL_GLContext gl_ctx;
    RtsVfs vfs;
    RtsWorld world;
    RtsInputRouter input;
    RtsVoxelChunk voxel;
    RtsAudioEngine audio;
    RtsEconomySystem economy;
    RtsCommanderController ctrl;
    RtsBoardingSystem boarding;
    RtsTacticalSim tactical;
    RtsRetroRenderer renderer;
    RtsLuaVm lua;
    RtsShowcaseScene scene;
    RtsLiveViewMode view_mode;
    bool web_touch_latched;
    char quicksave_buf[32768];
    bool has_quicksave;
    float time_s;
    bool running;
} InteractiveApp;

static bool init_sdl_gl(SDL_Window **out_win, SDL_GLContext *out_ctx, int w, int h, bool hidden) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        fprintf(stderr, "[platform_sdl] SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }
#ifdef __EMSCRIPTEN__
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    Uint64 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    if (hidden) {
        flags |= SDL_WINDOW_HIDDEN;
    }
    SDL_Window *win = SDL_CreateWindow("1stPRTS (1prtsg) - First-Person RTS", w, h, flags);
    if (!win) {
        fprintf(stderr, "[platform_sdl] SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return false;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext(win);
    if (!ctx) {
        fprintf(stderr, "[platform_sdl] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return false;
    }
    SDL_GL_MakeCurrent(win, ctx);
    SDL_GL_SetSwapInterval(0);
    *out_win = win;
    *out_ctx = ctx;
    return true;
}

static bool init_engine_stack(InteractiveApp *app, const RtsAppConfig *cfg) {
    const char *base = cfg->data_dir ? cfg->data_dir : SDL_GetBasePath();
    if (!base) base = ".";
    if (!rts_vfs_init(&app->vfs, base, NULL)) {
        fprintf(stderr, "[platform_sdl] VFS init failed for '%s'\n", base);
        return false;
    }
    uint64_t seed = cfg->seed != 0u ? cfg->seed : 42u;
    rts_world_init(&app->world, rts_planet_earthlike(42u), seed);
    RtsEngineConfig ecfg;
    if (rts_config_load_vfs(&app->vfs, "engine.cfg", &ecfg)) {
        rts_config_apply(&ecfg, &app->vfs, &app->world);
    }
    if (cfg->mod_to_mount && cfg->mod_to_mount[0] != '\0') {
        rts_vfs_mount_mod(&app->vfs, cfg->mod_to_mount);
    }
    rts_input_init(&app->input, (float)cfg->width, (float)cfg->height, rts_web_is_touch_device());
    rts_voxel_init(&app->voxel, 1.0f);
    rts_audio_init(&app->audio, rts_audio_sdl3_create(), &app->vfs, 22050);
    rts_economy_init(&app->economy);
    rts_controller_init(&app->ctrl);
    rts_boarding_init(&app->boarding);
    RtsFeelConfig feel = cfg->fun_baseline ? rts_feel_config_baseline() : rts_feel_config_solaris();
    rts_tactical_init(&app->tactical, &feel, seed, &app->audio);

    if (!rts_renderer_init(&app->renderer, rts_gfx_gl_create(), &app->vfs, cfg->width, cfg->height)) {
        fprintf(stderr, "[platform_sdl] Renderer init failed\n");
        return false;
    }
    if (!rts_lua_init(&app->lua, &app->vfs, &app->world, &app->input, &app->voxel, &app->renderer)) {
        fprintf(stderr, "[platform_sdl] Lua VM init failed\n");
        return false;
    }
    rts_lua_attach_phase2(&app->lua, &app->audio, &app->economy, &app->ctrl);
    rts_lua_attach_phase3(&app->lua, &app->boarding);
    if (!rts_lua_exec_vfs_script(&app->lua, "main.lua")) {
        fprintf(stderr, "[platform_sdl] scripts/main.lua failed: %s\n", app->lua.last_error);
        return false;
    }
    /* Start Commander facing Citadel War Room Holo-Globe ready for Seamless Sandbox RTS */
    app->world.commander.local_pos = rts_v3d(0.0, 1.75, 0.2);
    app->world.commander.yaw_rad = 0.0;
    app->world.commander.pitch_rad = -0.05;
    rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS", "",
                                 "WALK WAR ROOM [WASD] | [E] HOLO-GLOBE/BOARD | [G] ASTROGATOR 400KM ORBIT | [F5/F9] SAVE/LOAD");
    return true;
}

static void shutdown_engine_stack(InteractiveApp *app) {
    rts_lua_shutdown(&app->lua);
    rts_audio_shutdown(&app->audio);
    rts_renderer_shutdown(&app->renderer);
    if (app->gl_ctx) SDL_GL_DestroyContext(app->gl_ctx);
    if (app->window) SDL_DestroyWindow(app->window);
    SDL_Quit();
}

static uint32_t find_entity_by_blueprint(const RtsWorld *w, const char *bp_id, uint8_t faction_id) {
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (faction_id != 0 && e->faction_id != faction_id) continue;
        if (strcmp(e->blueprint_id, bp_id) == 0) return e->id;
    }
    return 0u;
}

static void sandbox_trigger_astrogator(InteractiveApp *app) {
    uint32_t dropship_id = find_entity_by_blueprint(&app->world, "air_dropship", 1u);
    uint32_t shipyard_id = find_entity_by_blueprint(&app->world, "stn_orbital_shipyard", 1u);
    RtsEntity *ds = rts_world_get_entity(&app->world, dropship_id);
    if (!ds) return;

    /* Board Commander into Dropship Valkyrie-2 so the ascent/orbit is experienced in 1st person */
    if (ds->own_frame_id != RTS_FRAME_INVALID) {
        app->world.commander.frame_id = ds->own_frame_id;
        app->world.commander.occupied_entity_id = ds->id;
        app->world.commander.local_pos = rts_v3d(0.0, 1.7, -1.2);
        app->world.commander.at_holo_globe = false;
        app->ctrl.mode = RTS_CMD_ON_FOOT;
    }

    if (!ds->orbital_mode) {
        if (rts_astrogator_start_ascent(&app->ctrl, &app->world, dropship_id, 400000.0)) {
            rts_renderer_set_live_status(&app->renderer, "ASTROGATOR ASCENT -> 400KM LEO", "",
                                         "DROPSHIP VALKYRIE-2 BURNING FOR 400KM ORBITAL SHIPYARD RENDEZVOUS!");
        }
    } else if (!app->ctrl.astrogator.docked && shipyard_id != 0u) {
        if (rts_astrogator_start_rendezvous(&app->ctrl, &app->world, dropship_id, shipyard_id)) {
            rts_renderer_set_live_status(&app->renderer, "ASTROGATOR RENDEZVOUS", "",
                                         "CLOSING WITH ORBITAL SHIPYARD ALPHA RING DRYDOCK AT 400KM LEO!");
        }
    } else {
        if (rts_astrogator_start_reentry(&app->ctrl, &app->world, dropship_id)) {
            rts_renderer_set_live_status(&app->renderer, "SHIELDED RE-ENTRY", "",
                                         "DROPSHIP VALKYRIE-2 EXECUTING SHIELDED ATMOSPHERIC RE-ENTRY TO HELIOS PRIME!");
        }
    }
}

static void sandbox_try_board_nearest(InteractiveApp *app) {
    if (app->world.commander.frame_id != app->world.planet_frame_id) {
        /* Step out onto planetary apron beside current construct */
        RtsEntity *cur = rts_world_get_entity(&app->world, app->world.commander.occupied_entity_id);
        if (cur && !cur->orbital_mode) {
            app->world.commander.frame_id = app->world.planet_frame_id;
            app->world.commander.occupied_entity_id = 0u;
            app->world.commander.local_pos = rts_v3d(cur->pos.x + 18.0, cur->pos.y + 1.75, cur->pos.z + 18.0);
            app->world.commander.at_holo_globe = false;
            app->ctrl.mode = RTS_CMD_ON_FOOT;
            rts_renderer_set_live_status(&app->renderer, "PLANETARY SURFACE EVA", "",
                                         "EXITED TO HELIOS PRIME APRON - WALK TO DROPSHIP OR PRESS [B] TO BOARD NEAREST CONSTRUCT.");
        }
        return;
    }

    RtsVec3d cw = app->world.commander.local_pos;
    uint32_t best_id = 0u;
    double best_d = 120.0;
    for (size_t i = 0; i < app->world.entity_count; ++i) {
        const RtsEntity *e = &app->world.entities[i];
        if (!e->active || e->faction_id != 1u || e->own_frame_id == RTS_FRAME_INVALID) continue;
        double d = rts_v3d_dist(e->pos, cw);
        if (d < best_d) {
            best_d = d;
            best_id = e->id;
        }
    }
    RtsEntity *tgt = rts_world_get_entity(&app->world, best_id);
    if (tgt) {
        app->world.commander.frame_id = tgt->own_frame_id;
        app->world.commander.occupied_entity_id = tgt->id;
        app->world.commander.local_pos = rts_v3d(0.0, 1.75, 0.2);
        app->world.commander.at_holo_globe = false;
        char msg[160];
        snprintf(msg, sizeof(msg), "BOARDED %s INTERIOR DECK - PRESS [E] AT CONSOLES OR [G] FOR ORBIT!", tgt->name);
        rts_renderer_set_live_status(&app->renderer, "INTERIOR DECK", "", msg);
    }
}

static void sandbox_exec_hologlobe_order(InteractiveApp *app, int slot) {
    if (slot == 1) {
        uint32_t foundry = find_entity_by_blueprint(&app->world, "bldg_vehicle_foundry", 1u);
        uint32_t job = rts_economy_queue_build(&app->economy, &app->world, foundry, "veh_plasma_tank");
        if (job > 0u) {
            rts_renderer_set_live_status(&app->renderer, "HOLO-GLOBE COMMAND", "",
                                         "ORDER [1]: QUEUED PLASMA HEAVY TANK AT SOLARIS VEHICLE FOUNDRY!");
        }
    } else if (slot == 2) {
        uint32_t tank = find_entity_by_blueprint(&app->world, "veh_plasma_tank", 1u);
        uint32_t enemy = find_entity_by_blueprint(&app->world, "veh_plasma_tank", 2u);
        if (tank != 0u && enemy != 0u) {
            rts_world_issue_order(&app->world, tank, RTS_ORDER_ATTACK, rts_v3d(0, 0, 0), enemy, 0.0);
            rts_renderer_set_live_status(&app->renderer, "HOLO-GLOBE COMMAND", "",
                                         "ORDER [2]: VANGUARD TANK 1 ENGAGING CRIMSON RAIDER 1!");
        }
    } else if (slot == 3) {
        sandbox_trigger_astrogator(app);
    } else if (slot == 4) {
        uint32_t edread = find_entity_by_blueprint(&app->world, "space_siege_dreadnought", 2u);
        if (edread != 0u) {
            uint32_t op = rts_boarding_start(&app->boarding, &app->world, edread, 1u, 140u, true);
            if (op > 0u) {
                rts_renderer_set_live_status(&app->renderer, "ORBITAL BOARDING OPS", "",
                                             "ORDER [4]: 140 SOLAR MARINES BREACHING CRIMSON SIEGE DREADNOUGHT AT 400KM LEO!");
            }
        }
    }
}

static void interactive_tick(void *arg) {
    InteractiveApp *app = (InteractiveApp *)arg;
    rts_input_begin_frame(&app->input);

    if (!app->web_touch_latched && rts_web_is_touch_device()) {
        rts_input_set_mode_override(&app->input, true, RTS_INPUT_MOBILE_TOUCH);
        app->web_touch_latched = true;
    }

    RtsTacticalInput tin;
    memset(&tin, 0, sizeof(tin));
    tin.aim_yaw = app->tactical.player_yaw;
    tin.aim_pitch = app->tactical.player_pitch;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            app->running = false;
        } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
            rts_input_notify_kbm_activity(&app->input);
            if (ev.key.key == SDLK_ESCAPE) app->running = false;
            if (ev.key.key == SDLK_L) {
                app->view_mode = (app->view_mode == RTS_VIEW_TACTICAL) ? RTS_VIEW_SANDBOX_RTS : RTS_VIEW_TACTICAL;
            }
            if (ev.key.key == SDLK_M) {
                app->view_mode = RTS_VIEW_SANDBOX_RTS;
                rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS", "",
                                             "SEAMLESS PLANETARY-TO-ORBITAL RTS ACTIVE: [E] CONSOLE | [B] BOARD/EXIT | [G] 400KM ORBIT | [F5/F9] SAVE");
            }
            if (ev.key.key == SDLK_E) app->input.intent.interact_pressed = true;
            if (ev.key.key == SDLK_TAB) app->input.intent.toggle_pad_pressed = true;
            if (ev.key.key == SDLK_G && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                sandbox_trigger_astrogator(app);
            }
            if (ev.key.key == SDLK_B && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                sandbox_try_board_nearest(app);
            }
            if (ev.key.key == SDLK_F5) {
                size_t n = rts_save_serialize(&app->world, &app->economy, app->quicksave_buf, sizeof(app->quicksave_buf));
                app->has_quicksave = (n > 0u);
                rts_renderer_set_live_status(&app->renderer, "RTS_SAVE_V1 QUICKSAVE", "",
                                             app->has_quicksave ? "QUICKSAVE WRITTEN (RTS_SAVE_V1) - SPATIAL FRAME & FLEET SNAPSHOTTED!"
                                                                : "QUICKSAVE FAILED.");
            }
            if (ev.key.key == SDLK_F9 && app->has_quicksave) {
                bool ok = rts_save_deserialize(app->quicksave_buf, &app->world, &app->economy);
                rts_renderer_set_live_status(&app->renderer, "RTS_SAVE_V1 QUICKLOAD", "",
                                             ok ? "QUICKLOAD RESTORED (RTS_SAVE_V1) - COMMANDER FRAME & FLEET SYNCED!"
                                                : "QUICKLOAD FAILED.");
            }
            if (ev.key.key == SDLK_J) tin.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
            if (ev.key.key == SDLK_Q) tin.action_pressed[RTS_ACT_EMP_PULSE] = true;
            if (ev.key.key == SDLK_LSHIFT || ev.key.key == SDLK_RSHIFT) tin.action_pressed[RTS_ACT_THRUSTER_DASH] = true;
            if (ev.key.key == SDLK_SPACE) tin.jump_pressed = true;
            if (ev.key.key == SDLK_F) tin.action_pressed[RTS_ACT_FLEET_STRIKE] = true;
            if (ev.key.key == SDLK_H) tin.action_pressed[RTS_ACT_FIELD_REPAIR] = true;
            if (ev.key.key == SDLK_R || ev.key.key == SDLK_RETURN) tin.retry_pressed = true;

            if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
                if (ev.key.key == SDLK_1) sandbox_exec_hologlobe_order(app, 1);
                if (ev.key.key == SDLK_2) sandbox_exec_hologlobe_order(app, 2);
                if (ev.key.key == SDLK_3) sandbox_exec_hologlobe_order(app, 3);
                if (ev.key.key == SDLK_4) sandbox_exec_hologlobe_order(app, 4);
            } else {
                if (ev.key.key == SDLK_1) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE; }
                if (ev.key.key == SDLK_2) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_PLANETARY_SURFACE; }
                if (ev.key.key == SDLK_3) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_DROPSHIP_INTERIOR; }
                if (ev.key.key == SDLK_4) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_ORBITAL_SHIPYARD; }
                if (ev.key.key == SDLK_5) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_MOBILE_TOUCH_HUD; }
                if (ev.key.key == SDLK_6) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_NAVAL_CARRIER_DECK; }
                if (ev.key.key == SDLK_7) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_FOUNDRY_INTERIOR; }
                if (ev.key.key == SDLK_8) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT; }
                if (ev.key.key == SDLK_9) { app->view_mode = RTS_VIEW_SHOWCASE; app->scene = RTS_SCENE_WEBXR_STEREO_HOLOGLOBE; }
            }
        } else if (ev.type == SDL_EVENT_MOUSE_MOTION) {
            tin.aim_yaw += ev.motion.xrel * 0.0045f;
            tin.aim_pitch = rts_clampf(tin.aim_pitch - ev.motion.yrel * 0.0045f, -1.1f, 1.1f);
            app->input.intent.look_dx += ev.motion.xrel * 0.0045f;
            app->input.intent.look_dy -= ev.motion.yrel * 0.0045f;
        } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN && ev.button.button == SDL_BUTTON_LEFT) {
            tin.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
        } else if (ev.type == SDL_EVENT_FINGER_DOWN) {
            rts_input_on_touch_down(&app->input, (int64_t)ev.tfinger.fingerID, ev.tfinger.x, ev.tfinger.y);
        } else if (ev.type == SDL_EVENT_FINGER_MOTION) {
            rts_input_on_touch_move(&app->input, (int64_t)ev.tfinger.fingerID, ev.tfinger.x, ev.tfinger.y);
        } else if (ev.type == SDL_EVENT_FINGER_UP) {
            rts_input_on_touch_up(&app->input, (int64_t)ev.tfinger.fingerID);
        }
    }

    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys) {
        if (keys[SDL_SCANCODE_W]) { app->input.intent.move_y = 1.0f; tin.move_z = 1.0f; }
        if (keys[SDL_SCANCODE_S]) { app->input.intent.move_y = -1.0f; tin.move_z = -1.0f; }
        if (keys[SDL_SCANCODE_A]) { app->input.intent.move_x = -1.0f; tin.move_x = -1.0f; }
        if (keys[SDL_SCANCODE_D]) { app->input.intent.move_x = 1.0f; tin.move_x = 1.0f; }
        if (keys[SDL_SCANCODE_LEFT])  { tin.aim_yaw -= 0.04f; app->input.intent.look_dx -= 0.04f; }
        if (keys[SDL_SCANCODE_RIGHT]) { tin.aim_yaw += 0.04f; app->input.intent.look_dx += 0.04f; }
    }

    RtsXrFrameState xr;
    bool xr_on = rts_webxr_get_latest_state(&xr) && xr.session_active;
    if (xr_on) {
        rts_input_update_xr(&app->input, &xr);
    }

    app->time_s += 1.0f / 60.0f;
    if (app->view_mode == RTS_VIEW_TACTICAL) {
        rts_tactical_step(&app->tactical, &tin, 1.0 / 60.0);
        rts_tactical_sync_to_world(&app->tactical, &app->world);
    } else {
        rts_controller_step(&app->ctrl, &app->world, &app->input.intent, 1.0 / 60.0);
        if (app->ctrl.astrogator.phase != RTS_AUTO_IDLE) {
            RtsAutopilotPhase ph = rts_astrogator_step(&app->ctrl, &app->world, 1.0 / 60.0);
            if (ph == RTS_AUTO_CIRCULARIZED) {
                rts_renderer_set_live_status(&app->renderer, "400KM LEO ORBIT CIRCULARIZED", "",
                                             "ORBIT ACHIEVED! PRESS [G] TO DOCK AT ORBITAL SHIPYARD ALPHA OR RE-ENTER.");
            } else if (ph == RTS_AUTO_LANDED) {
                rts_renderer_set_live_status(&app->renderer, "SURFACE TOUCHDOWN COMPLETE", "",
                                             "DROPSHIP VALKYRIE-2 SAFELY LANDED ON HELIOS PRIME! PRESS [B] TO EXIT.");
            }
        }
    }
    rts_economy_step(&app->economy, &app->world, 1.0 / 60.0);
    rts_boarding_step(&app->boarding, &app->world, 1.0 / 60.0);
    rts_world_step(&app->world, 1.0 / 60.0);

    if (xr_on) {
        rts_renderer_render_webxr_stereo(&app->renderer, &app->world, &app->input, &xr, app->time_s);
    } else if (app->view_mode == RTS_VIEW_TACTICAL) {
        rts_renderer_render_tactical(&app->renderer, &app->tactical, &app->world, &app->input, app->time_s);
    } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS) {
        rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, app->time_s);
    } else {
        rts_renderer_render_scene(&app->renderer, &app->world, &app->input, app->scene, app->time_s);
    }
    SDL_GL_SwapWindow(app->window);
}

int rts_platform_run_interactive(const RtsAppConfig *cfg) {
    InteractiveApp *app = (InteractiveApp *)calloc(1, sizeof(InteractiveApp));
    if (!app) return 1;
    app->scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE;
    app->view_mode = RTS_VIEW_TACTICAL;
    app->running = true;

    if (!init_sdl_gl(&app->window, &app->gl_ctx, cfg->width, cfg->height, false)) {
        free(app);
        return 1;
    }
    if (!init_engine_stack(app, cfg)) {
        shutdown_engine_stack(app);
        free(app);
        return 1;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(interactive_tick, app, 0, 1);
    return 0;
#else
    while (app->running) {
        interactive_tick(app);
        SDL_Delay(16);
    }
    shutdown_engine_stack(app);
    free(app);
    return 0;
#endif
}

static bool save_tactical_frame(InteractiveApp *app, const char *dir, const char *name, float sim_t, double *out_ms) {
    Uint64 t0 = SDL_GetPerformanceCounter();
    rts_renderer_render_tactical(&app->renderer, &app->tactical, &app->world, &app->input, sim_t);
    SDL_GL_SwapWindow(app->window);
    rts_renderer_render_tactical(&app->renderer, &app->tactical, &app->world, &app->input, sim_t);
    Uint64 t1 = SDL_GetPerformanceCounter();
    if (out_ms) {
        *out_ms = (double)(t1 - t0) * 500.0 / (double)SDL_GetPerformanceFrequency();
    }
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    bool ok = rts_renderer_capture_png(&app->renderer, path);
    SDL_GL_SwapWindow(app->window);
    if (ok) {
        printf("[fun_frames] Saved %s (render %.2f ms)\n", path, out_ms ? *out_ms : 0.0);
    }
    return ok;
}

int rts_platform_capture_fun_frames(const RtsAppConfig *cfg) {
    InteractiveApp *app = (InteractiveApp *)calloc(1, sizeof(InteractiveApp));
    if (!app) return 1;

    if (!init_sdl_gl(&app->window, &app->gl_ctx, cfg->width, cfg->height, false)) {
        free(app);
        return 1;
    }
    if (!init_engine_stack(app, cfg)) {
        shutdown_engine_stack(app);
        free(app);
        return 1;
    }

    int rc = 0;
    double render_ms = 0.0;
    uint64_t bot_rng = (cfg->seed != 0u ? cfg->seed : 42u) ^ 0xA5A5A5A5u;

    RtsTacticalInput init_in;
    rts_fun_bot_decide(RTS_BOT_HUMAN_PROXY, &app->tactical, &bot_rng, &init_in);
    rts_tactical_step(&app->tactical, &init_in, 1.0 / 60.0);
    rts_tactical_sync_to_world(&app->tactical, &app->world);
    if (!save_tactical_frame(app, cfg->fun_frames_dir, "fun_01_boot.png", 0.1f, &render_ms)) rc = 1;

    for (int tick = 0; tick < 1800 && app->tactical.active_run; ++tick) {
        RtsTacticalInput in;
        rts_fun_bot_decide(RTS_BOT_HUMAN_PROXY, &app->tactical, &bot_rng, &in);
        rts_tactical_step(&app->tactical, &in, 1.0 / 60.0);
        if (tick > 420 && rts_tactical_active_enemy_count(&app->tactical) >= 3u &&
            rts_tactical_active_particle_count(&app->tactical) >= 10u) {
            break;
        }
    }
    rts_tactical_sync_to_world(&app->tactical, &app->world);
    if (!save_tactical_frame(app, cfg->fun_frames_dir, "fun_02_peak_tension.png", (float)app->tactical.sim_time_s, &render_ms)) rc = 1;

    RtsFeelConfig go_cfg = rts_feel_config_solaris();
    go_cfg.enemy_dmg_scale = 2.6;
    rts_tactical_init(&app->tactical, &go_cfg, 108u, &app->audio);
    for (int tick = 0; tick < 3600 && !app->tactical.game_over; ++tick) {
        RtsTacticalInput in;
        rts_fun_bot_decide(RTS_BOT_NOVICE, &app->tactical, &bot_rng, &in);
        rts_tactical_step(&app->tactical, &in, 1.0 / 60.0);
    }
    rts_tactical_sync_to_world(&app->tactical, &app->world);
    if (!save_tactical_frame(app, cfg->fun_frames_dir, "fun_03_game_over.png", (float)app->tactical.sim_time_s, &render_ms)) rc = 1;

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[fun_frames] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) rc = 1;

    shutdown_engine_stack(app);
    free(app);
    return rc;
}

int rts_platform_capture_roster_gallery(const RtsAppConfig *cfg) {
    InteractiveApp *app = (InteractiveApp *)calloc(1, sizeof(InteractiveApp));
    if (!app) return 1;

    if (!init_sdl_gl(&app->window, &app->gl_ctx, cfg->width, cfg->height, false)) {
        free(app);
        return 1;
    }
    if (!init_engine_stack(app, cfg)) {
        shutdown_engine_stack(app);
        free(app);
        return 1;
    }

    static const struct {
        RtsConstructClass cls;
        const char *filename;
        float sim_time;
    } galleries[6] = {
        {RTS_CLASS_BUILDING,       "gallery_01_buildings_24.png",  0.4f},
        {RTS_CLASS_GROUND_VEHICLE, "gallery_02_vehicles_10.png",   0.7f},
        {RTS_CLASS_NAVAL_SHIP,     "gallery_03_naval_10.png",      1.0f},
        {RTS_CLASS_AIRCRAFT,       "gallery_04_aircraft_8.png",    1.3f},
        {RTS_CLASS_SPACECRAFT,     "gallery_05_spacecraft_14.png", 1.6f},
        {RTS_CLASS_SPACE_STATION,  "gallery_06_stations_10.png",   1.9f}
    };

    int rc = 0;
    for (int i = 0; i < 6; ++i) {
        rts_renderer_render_roster_class(&app->renderer, &app->world, &app->input, galleries[i].cls, galleries[i].sim_time);
        SDL_GL_SwapWindow(app->window);
        rts_renderer_render_roster_class(&app->renderer, &app->world, &app->input, galleries[i].cls, galleries[i].sim_time);

        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", cfg->roster_gallery_dir, galleries[i].filename);
        if (!rts_renderer_capture_png(&app->renderer, out_path)) {
            fprintf(stderr, "[gallery] Failed to write %s\n", out_path);
            rc = 1;
        } else {
            printf("[gallery] Saved %s\n", out_path);
        }
        SDL_GL_SwapWindow(app->window);
    }

    /* Also capture live Seamless Sandbox RTS Mode inside the hydrated Citadel War Room with Holo-Globe active */
    app->world.commander.local_pos = rts_v3d(0.0, 2.0, -0.4);
    app->world.commander.yaw_rad = 0.04;
    app->world.commander.pitch_rad = -0.04;
    app->world.commander.at_holo_globe = true;
    rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS [MODE M]", "",
                                 "LIVE CITADEL WAR ROOM: [1] QUEUE TANK | [2] ATTACK | [3] 400KM ORBIT | [4] BOARDING | [F5/F9] SAVE");
    rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, 2.2f);
    SDL_GL_SwapWindow(app->window);
    rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, 2.2f);
    char live_path[512];
    snprintf(live_path, sizeof(live_path), "%s/gallery_07_seamless_sandbox_rts.png", cfg->roster_gallery_dir);
    if (!rts_renderer_capture_png(&app->renderer, live_path)) {
        fprintf(stderr, "[gallery] Failed to write %s\n", live_path);
        rc = 1;
    } else {
        printf("[gallery] Saved %s\n", live_path);
    }
    SDL_GL_SwapWindow(app->window);

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[gallery] Cached unique models: %zu, draw calls: %u, screenshots: %u, gl_errors: %u\n",
           app->renderer.mesh_cache_count, st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u || app->renderer.mesh_cache_count < 76u) {
        fprintf(stderr, "[gallery] FAIL: gl_errors=%u, cached=%zu\n", st.gl_errors, app->renderer.mesh_cache_count);
        rc = 1;
    }

    shutdown_engine_stack(app);
    free(app);
    return rc;
}

int rts_platform_capture_screenshots(const RtsAppConfig *cfg) {
    InteractiveApp *app = (InteractiveApp *)calloc(1, sizeof(InteractiveApp));
    if (!app) return 1;

    if (!init_sdl_gl(&app->window, &app->gl_ctx, cfg->width, cfg->height, false)) {
        free(app);
        return 1;
    }
    if (!init_engine_stack(app, cfg)) {
        shutdown_engine_stack(app);
        free(app);
        return 1;
    }

    static const struct {
        RtsShowcaseScene scene;
        const char *filename;
        float sim_time;
    } shots[RTS_SCENE_COUNT] = {
        {RTS_SCENE_WAR_ROOM_HOLOGLOBE,           "01_war_room_hologlobe.png", 1.2f},
        {RTS_SCENE_PLANETARY_SURFACE,            "02_planetary_surface_1to1.png", 2.4f},
        {RTS_SCENE_DROPSHIP_INTERIOR,            "03_dropship_interior_ascent.png", 3.6f},
        {RTS_SCENE_ORBITAL_SHIPYARD,             "04_orbital_shipyard_dreadnought.png", 4.8f},
        {RTS_SCENE_MOBILE_TOUCH_HUD,             "05_mobile_web_touch_hud.png", 1.6f},
        {RTS_SCENE_NAVAL_CARRIER_DECK,           "06_naval_carrier_flight_deck.png", 2.8f},
        {RTS_SCENE_FOUNDRY_INTERIOR,             "07_foundry_assembly_wristpad.png", 3.2f},
        {RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT,"08_dreadnought_redalert_boarding.png", 4.1f},
        {RTS_SCENE_WEBXR_STEREO_HOLOGLOBE,       "09_webxr_stereo_vr_view.png", 1.8f}
    };

    int rc = 0;
    for (int i = 0; i < RTS_SCENE_COUNT; ++i) {
        if (shots[i].scene == RTS_SCENE_MOBILE_TOUCH_HUD) {
            rts_input_set_mode_override(&app->input, true, RTS_INPUT_MOBILE_TOUCH);
            rts_input_on_touch_down(&app->input, 7, 0.14f, 0.80f);
            rts_input_on_touch_move(&app->input, 7, 0.19f, 0.74f);
        } else {
            rts_input_set_mode_override(&app->input, false, RTS_INPUT_DESKTOP_KBM);
        }
        rts_renderer_render_scene(&app->renderer, &app->world, &app->input, shots[i].scene, shots[i].sim_time);
        SDL_GL_SwapWindow(app->window);
        rts_renderer_render_scene(&app->renderer, &app->world, &app->input, shots[i].scene, shots[i].sim_time);

        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", cfg->screenshot_dir, shots[i].filename);
        if (!rts_renderer_capture_png(&app->renderer, out_path)) {
            fprintf(stderr, "[screenshots] Failed to write %s\n", out_path);
            rc = 1;
        } else {
            printf("[screenshots] Saved %s\n", out_path);
        }
        SDL_GL_SwapWindow(app->window);
    }

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[screenshots] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) {
        fprintf(stderr, "[screenshots] FAIL: OpenGL reported %u errors\n", st.gl_errors);
        rc = 1;
    }

    shutdown_engine_stack(app);
    free(app);
    return rc;
}
