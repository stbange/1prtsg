/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/lua_vm.h"
#include "rts/platform.h"
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
    RtsRetroRenderer renderer;
    RtsLuaVm lua;
    RtsShowcaseScene scene;
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
    if (cfg->mod_to_mount && cfg->mod_to_mount[0] != '\0') {
        rts_vfs_mount_mod(&app->vfs, cfg->mod_to_mount);
    }
    rts_world_init(&app->world, rts_planet_earthlike(42u), 1337u);
    rts_input_init(&app->input, (float)cfg->width, (float)cfg->height, false);
    rts_voxel_init(&app->voxel, 1.0f);

    if (!rts_renderer_init(&app->renderer, rts_gfx_gl_create(), &app->vfs, cfg->width, cfg->height)) {
        fprintf(stderr, "[platform_sdl] Renderer init failed\n");
        return false;
    }
    if (!rts_lua_init(&app->lua, &app->vfs, &app->world, &app->input, &app->voxel, &app->renderer)) {
        fprintf(stderr, "[platform_sdl] Lua VM init failed\n");
        return false;
    }
    if (!rts_lua_exec_vfs_script(&app->lua, "main.lua")) {
        fprintf(stderr, "[platform_sdl] scripts/main.lua failed: %s\n", app->lua.last_error);
        return false;
    }
    return true;
}

static void shutdown_engine_stack(InteractiveApp *app) {
    rts_lua_shutdown(&app->lua);
    rts_renderer_shutdown(&app->renderer);
    if (app->gl_ctx) SDL_GL_DestroyContext(app->gl_ctx);
    if (app->window) SDL_DestroyWindow(app->window);
    SDL_Quit();
}

static void interactive_tick(void *arg) {
    InteractiveApp *app = (InteractiveApp *)arg;
    rts_input_begin_frame(&app->input);

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            app->running = false;
        } else if (ev.type == SDL_EVENT_KEY_DOWN) {
            rts_input_notify_kbm_activity(&app->input);
            if (ev.key.key == SDLK_ESCAPE) app->running = false;
            if (ev.key.key == SDLK_1) app->scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE;
            if (ev.key.key == SDLK_2) app->scene = RTS_SCENE_PLANETARY_SURFACE;
            if (ev.key.key == SDLK_3) app->scene = RTS_SCENE_DROPSHIP_INTERIOR;
            if (ev.key.key == SDLK_4) app->scene = RTS_SCENE_ORBITAL_SHIPYARD;
            if (ev.key.key == SDLK_5) app->scene = RTS_SCENE_MOBILE_TOUCH_HUD;
        } else if (ev.type == SDL_EVENT_FINGER_DOWN) {
            rts_input_on_touch_down(&app->input, (int64_t)ev.tfinger.fingerID, ev.tfinger.x, ev.tfinger.y);
        } else if (ev.type == SDL_EVENT_FINGER_MOTION) {
            rts_input_on_touch_move(&app->input, (int64_t)ev.tfinger.fingerID, ev.tfinger.x, ev.tfinger.y);
        } else if (ev.type == SDL_EVENT_FINGER_UP) {
            rts_input_on_touch_up(&app->input, (int64_t)ev.tfinger.fingerID);
        }
    }

    RtsXrFrameState xr;
    if (rts_webxr_get_latest_state(&xr) && xr.session_active) {
        rts_input_update_xr(&app->input, &xr);
    }

    app->time_s += 1.0f / 60.0f;
    rts_world_step(&app->world, 1.0 / 60.0);
    rts_renderer_render_scene(&app->renderer, &app->world, &app->input, app->scene, app->time_s);
    SDL_GL_SwapWindow(app->window);
}

int rts_platform_run_interactive(const RtsAppConfig *cfg) {
    InteractiveApp app;
    memset(&app, 0, sizeof(app));
    app.scene = RTS_SCENE_WAR_ROOM_HOLOGLOBE;
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
        {RTS_SCENE_WAR_ROOM_HOLOGLOBE, "01_war_room_hologlobe.png", 1.2f},
        {RTS_SCENE_PLANETARY_SURFACE,  "02_planetary_surface_1to1.png", 2.4f},
        {RTS_SCENE_DROPSHIP_INTERIOR,  "03_dropship_interior_ascent.png", 3.6f},
        {RTS_SCENE_ORBITAL_SHIPYARD,   "04_orbital_shipyard_dreadnought.png", 4.8f},
        {RTS_SCENE_MOBILE_TOUCH_HUD,   "05_mobile_web_touch_hud.png", 1.6f}
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
        /* Render two frames to guarantee fresh backbuffer before glReadPixels */
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
