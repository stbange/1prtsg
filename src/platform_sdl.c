/* SPDX-License-Identifier: MIT */
#include "rts/fun_eval.h"
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/lua_vm.h"
#include "rts/platform.h"
#include "rts/tactical_sim.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

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
    bool live_camera_mode;
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
    rts_input_init(&app->input, (float)cfg->width, (float)cfg->height, false);
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

static void interactive_tick(void *arg) {
    InteractiveApp *app = (InteractiveApp *)arg;
    rts_input_begin_frame(&app->input);

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
            if (ev.key.key == SDLK_L) app->live_camera_mode = !app->live_camera_mode;
            if (ev.key.key == SDLK_E) app->input.intent.interact_pressed = true;
            if (ev.key.key == SDLK_TAB) app->input.intent.toggle_pad_pressed = true;
            if (ev.key.key == SDLK_J) tin.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
            if (ev.key.key == SDLK_Q) tin.action_pressed[RTS_ACT_EMP_PULSE] = true;
            if (ev.key.key == SDLK_LSHIFT || ev.key.key == SDLK_RSHIFT) tin.action_pressed[RTS_ACT_THRUSTER_DASH] = true;
            if (ev.key.key == SDLK_SPACE) tin.jump_pressed = true;
            if (ev.key.key == SDLK_F) tin.action_pressed[RTS_ACT_FLEET_STRIKE] = true;
            if (ev.key.key == SDLK_H) tin.action_pressed[RTS_ACT_FIELD_REPAIR] = true;
            if (ev.key.key == SDLK_R || ev.key.key == SDLK_RETURN) tin.retry_pressed = true;
            if (ev.key.key == SDLK_1) { app->live_camera_mode = false; app->scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE; }
            if (ev.key.key == SDLK_2) { app->live_camera_mode = false; app->scene = RTS_SCENE_PLANETARY_SURFACE; }
            if (ev.key.key == SDLK_3) { app->live_camera_mode = false; app->scene = RTS_SCENE_DROPSHIP_INTERIOR; }
            if (ev.key.key == SDLK_4) { app->live_camera_mode = false; app->scene = RTS_SCENE_ORBITAL_SHIPYARD; }
            if (ev.key.key == SDLK_5) { app->live_camera_mode = false; app->scene = RTS_SCENE_MOBILE_TOUCH_HUD; }
            if (ev.key.key == SDLK_6) { app->live_camera_mode = false; app->scene = RTS_SCENE_NAVAL_CARRIER_DECK; }
            if (ev.key.key == SDLK_7) { app->live_camera_mode = false; app->scene = RTS_SCENE_FOUNDRY_INTERIOR; }
            if (ev.key.key == SDLK_8) { app->live_camera_mode = false; app->scene = RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT; }
            if (ev.key.key == SDLK_9) { app->live_camera_mode = false; app->scene = RTS_SCENE_WEBXR_STEREO_HOLOGLOBE; }
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

    /* Continuous responsive keyboard sampling (eliminates single-frame key-repeat stuttering) */
    const bool *keys = SDL_GetKeyboardState(NULL);
    if (keys) {
        if (keys[SDL_SCANCODE_W]) { app->input.intent.move_y = 1.0f; tin.move_z = 1.0f; }
        if (keys[SDL_SCANCODE_S]) { app->input.intent.move_y = -1.0f; tin.move_z = -1.0f; }
        if (keys[SDL_SCANCODE_A]) { app->input.intent.move_x = -1.0f; tin.move_x = -1.0f; }
        if (keys[SDL_SCANCODE_D]) { app->input.intent.move_x = 1.0f; tin.move_x = 1.0f; }
        if (keys[SDL_SCANCODE_LEFT])  tin.aim_yaw -= 0.04f;
        if (keys[SDL_SCANCODE_RIGHT]) tin.aim_yaw += 0.04f;
    }

    RtsXrFrameState xr;
    bool xr_on = rts_webxr_get_latest_state(&xr) && xr.session_active;
    if (xr_on) {
        rts_input_update_xr(&app->input, &xr);
    }

    app->time_s += 1.0f / 60.0f;
    if (app->live_camera_mode) {
        rts_tactical_step(&app->tactical, &tin, 1.0 / 60.0);
        rts_tactical_sync_to_world(&app->tactical, &app->world);
    } else {
        rts_controller_step(&app->ctrl, &app->world, &app->input.intent, 1.0 / 60.0);
    }
    rts_economy_step(&app->economy, &app->world, 1.0 / 60.0);
    rts_boarding_step(&app->boarding, &app->world, 1.0 / 60.0);
    rts_world_step(&app->world, 1.0 / 60.0);

    if (xr_on) {
        rts_renderer_render_webxr_stereo(&app->renderer, &app->world, &app->input, &xr, app->time_s);
    } else if (app->live_camera_mode) {
        rts_renderer_render_tactical(&app->renderer, &app->tactical, &app->world, &app->input, app->time_s);
    } else {
        rts_renderer_render_scene(&app->renderer, &app->world, &app->input, app->scene, app->time_s);
    }
    SDL_GL_SwapWindow(app->window);
}

int rts_platform_run_interactive(const RtsAppConfig *cfg) {
    InteractiveApp app;
    memset(&app, 0, sizeof(app));
    app.scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE;
    app.live_camera_mode = true; /* Fast Time-to-First-Action: drop straight into live Solaris Defense! */
    app.running = true;

    if (!init_sdl_gl(&app.window, &app.gl_ctx, cfg->width, cfg->height, false)) {
        return 1;
    }
    if (!init_engine_stack(&app, cfg)) {
        shutdown_engine_stack(&app);
        return 1;
    }

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(interactive_tick, &app, 0, 1);
#else
    while (app.running) {
        interactive_tick(&app);
        SDL_Delay(16);
    }
#endif

    shutdown_engine_stack(&app);
    return 0;
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
    InteractiveApp app;
    memset(&app, 0, sizeof(app));

    if (!init_sdl_gl(&app.window, &app.gl_ctx, cfg->width, cfg->height, false)) {
        return 1;
    }
    if (!init_engine_stack(&app, cfg)) {
        shutdown_engine_stack(&app);
        return 1;
    }

    int rc = 0;
    double render_ms = 0.0;
    uint64_t bot_rng = (cfg->seed != 0u ? cfg->seed : 42u) ^ 0xA5A5A5A5u;

    /* 1. BOOT frame (immediate interactive start facing incoming opening squad) */
    RtsTacticalInput init_in;
    rts_fun_bot_decide(RTS_BOT_HUMAN_PROXY, &app.tactical, &bot_rng, &init_in);
    rts_tactical_step(&app.tactical, &init_in, 1.0 / 60.0);
    rts_tactical_sync_to_world(&app.tactical, &app.world);
    if (!save_tactical_frame(&app, cfg->fun_frames_dir, "fun_01_boot.png", 0.1f, &render_ms)) rc = 1;

    /* 2. PEAK TENSION frame (advance live Human-Proxy bot until high-action EMP/Telegraph/Projectile moment) */
    for (int tick = 0; tick < 1800 && app.tactical.active_run; ++tick) {
        RtsTacticalInput in;
        rts_fun_bot_decide(RTS_BOT_HUMAN_PROXY, &app.tactical, &bot_rng, &in);
        rts_tactical_step(&app.tactical, &in, 1.0 / 60.0);
        if (tick > 420 && rts_tactical_active_enemy_count(&app.tactical) >= 3u &&
            rts_tactical_active_particle_count(&app.tactical) >= 10u) {
            break;
        }
    }
    rts_tactical_sync_to_world(&app.tactical, &app.world);
    if (!save_tactical_frame(&app, cfg->fun_frames_dir, "fun_02_peak_tension.png", (float)app.tactical.sim_time_s, &render_ms)) rc = 1;

    /* 3. GAME OVER frame (run Novice bot until authentic Ironman Defeat so retry modal is audited) */
    RtsFeelConfig go_cfg = rts_feel_config_solaris();
    go_cfg.enemy_dmg_scale = 2.6;
    rts_tactical_init(&app.tactical, &go_cfg, 108u, &app.audio);
    for (int tick = 0; tick < 3600 && !app.tactical.game_over; ++tick) {
        RtsTacticalInput in;
        rts_fun_bot_decide(RTS_BOT_NOVICE, &app.tactical, &bot_rng, &in);
        rts_tactical_step(&app.tactical, &in, 1.0 / 60.0);
    }
    rts_tactical_sync_to_world(&app.tactical, &app.world);
    if (!save_tactical_frame(&app, cfg->fun_frames_dir, "fun_03_game_over.png", (float)app.tactical.sim_time_s, &render_ms)) rc = 1;

    RtsGfxStats st = app.renderer.gfx.get_stats(app.renderer.gfx.state);
    printf("[fun_frames] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) rc = 1;

    shutdown_engine_stack(&app);
    return rc;
}

int rts_platform_capture_screenshots(const RtsAppConfig *cfg) {
    InteractiveApp app;
    memset(&app, 0, sizeof(app));

    if (!init_sdl_gl(&app.window, &app.gl_ctx, cfg->width, cfg->height, false)) {
        return 1;
    }
    if (!init_engine_stack(&app, cfg)) {
        shutdown_engine_stack(&app);
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
            rts_input_set_mode_override(&app.input, true, RTS_INPUT_MOBILE_TOUCH);
            rts_input_on_touch_down(&app.input, 7, 0.14f, 0.80f);
            rts_input_on_touch_move(&app.input, 7, 0.19f, 0.74f);
        } else {
            rts_input_set_mode_override(&app.input, false, RTS_INPUT_DESKTOP_KBM);
        }
        rts_renderer_render_scene(&app.renderer, &app.world, &app.input, shots[i].scene, shots[i].sim_time);
        SDL_GL_SwapWindow(app.window);
        rts_renderer_render_scene(&app.renderer, &app.world, &app.input, shots[i].scene, shots[i].sim_time);

        char out_path[512];
        snprintf(out_path, sizeof(out_path), "%s/%s", cfg->screenshot_dir, shots[i].filename);
        if (!rts_renderer_capture_png(&app.renderer, out_path)) {
            fprintf(stderr, "[screenshots] Failed to write %s\n", out_path);
            rc = 1;
        } else {
            printf("[screenshots] Saved %s\n", out_path);
        }
        SDL_GL_SwapWindow(app.window);
    }

    RtsGfxStats st = app.renderer.gfx.get_stats(app.renderer.gfx.state);
    printf("[screenshots] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) {
        fprintf(stderr, "[screenshots] FAIL: OpenGL reported %u errors\n", st.gl_errors);
        rc = 1;
    }

    shutdown_engine_stack(&app);
    return rc;
}
