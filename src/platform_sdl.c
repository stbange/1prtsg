/* SPDX-License-Identifier: MIT */
#include "rts/combat_sim.h"
#include "rts/crew.h"
#include "rts/field_gear.h"
#include "rts/fun_eval.h"
#include "rts/gfx.h"
#include "rts/hologlobe_ui.h"
#include "rts/input.h"
#include "rts/interior_gen.h"
#include "rts/lua_vm.h"
#include "rts/planet_mesh.h"
#include "rts/platform.h"
#include "rts/save_load.h"
#include "rts/tactical_sim.h"
#include "rts/vehicle_physics.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

typedef enum RtsLiveViewMode {
    RTS_VIEW_SHOWCASE = 0,
    RTS_VIEW_TACTICAL = 1,
    RTS_VIEW_SANDBOX_RTS = 2,
    RTS_VIEW_GALLERY = 3
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
    RtsMegaInterior mega;
    RtsCrewDeckSim crew;
    RtsHoloGlobeState holo;
    RtsFieldGearState gear;
    RtsCombatSim combat;
    RtsVehiclePhysicsState vphys;
    RtsRetroRenderer renderer;
    RtsLuaVm lua;
    RtsShowcaseScene scene;
    RtsConstructClass gallery_cls;
    RtsLiveViewMode view_mode;
    int showcase_ship_idx;
    int last_web_touch_state;
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

static uint32_t find_entity_by_blueprint(const RtsWorld *w, const char *bp_id, uint8_t faction_id) {
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (faction_id != 0 && e->faction_id != faction_id) continue;
        if (strcmp(e->blueprint_id, bp_id) == 0) return e->id;
    }
    return 0u;
}

static bool sandbox_enter_entity_mega(InteractiveApp *app, uint32_t entity_id, uint16_t deck) {
    RtsEntity *ent = rts_world_get_entity(&app->world, entity_id);
    if (!ent || ent->own_frame_id == RTS_FRAME_INVALID) return false;
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(&app->world, ent->blueprint_id);
    if (!bp) return false;

    if (!rts_mega_generate_for_blueprint(&app->mega, bp, ent->id)) return false;
    if (deck >= app->mega.deck_count) deck = 0u;
    if (!rts_mega_switch_deck(&app->mega, &app->world, deck)) return false;
    size_t staffed = rts_crew_populate_deck(&app->crew, &app->mega, &app->world);
    rts_renderer_set_active_interior(&app->renderer, &app->mega, &app->crew);

    app->ctrl.mode = RTS_CMD_ON_FOOT;
    app->ctrl.active_console_id = 0u;
    app->world.commander.yaw_rad = 0.0;
    app->world.commander.pitch_rad = -0.05;

    char tag[80];
    snprintf(tag, sizeof(tag), "MEGA-INTERIOR (%u ROOMS / %u DECKS)",
             (unsigned)app->mega.total_rooms, (unsigned)app->mega.deck_count);
    char toast[220];
    snprintf(toast, sizeof(toast),
             "%.36s DECK %u/%u | %zu LIVE STAFF | [WASD] WALK ROOMS | [/] LIFT | [N] NEXT SHIP | [F] RED ALERT",
             ent->name, (unsigned)(deck + 1u), (unsigned)app->mega.deck_count, staffed);
    rts_renderer_set_live_status(&app->renderer, tag, "", toast);
    return true;
}

static void sandbox_switch_deck(InteractiveApp *app, int delta) {
    if (app->mega.total_rooms == 0u || app->mega.deck_count <= 1u) {
        rts_renderer_set_live_status(&app->renderer, "SINGLE-DECK INTERIOR", "",
                                     "SINGLE-DECK CONSTRUCT - PRESS [N] TO TOUR MULTI-DECK CARRIERS & 420-ROOM DREADNOUGHTS!");
        return;
    }
    int next = ((int)app->mega.active_deck + delta + (int)app->mega.deck_count) % (int)app->mega.deck_count;
    if (rts_mega_switch_deck(&app->mega, &app->world, (uint16_t)next)) {
        size_t staffed = rts_crew_populate_deck(&app->crew, &app->mega, &app->world);
        rts_renderer_set_active_interior(&app->renderer, &app->mega, &app->crew);
        app->ctrl.mode = RTS_CMD_ON_FOOT;
        app->world.commander.yaw_rad = 0.0;
        app->world.commander.pitch_rad = -0.04;
        char tag[80];
        snprintf(tag, sizeof(tag), "TURBOLIFT DECK %u / %u",
                 (unsigned)(next + 1), (unsigned)app->mega.deck_count);
        char toast[220];
        snprintf(toast, sizeof(toast),
                 "ARRIVED AT DECK %u/%u (%u TOTAL ROOMS) | %zu STAFFED CREW ACTIVE | [WASD] EXPLORE | [/] TURBOLIFT",
                 (unsigned)(next + 1), (unsigned)app->mega.deck_count,
                 (unsigned)app->mega.total_rooms, staffed);
        rts_renderer_set_live_status(&app->renderer, tag, "", toast);
    }
}

static void sandbox_cycle_megaship(InteractiveApp *app) {
    static const char *const showcase_bps[5] = {
        "bldg_command_citadel",
        "air_dropship",
        "ship_fleet_carrier",
        "space_command_dreadnought",
        "stn_orbital_shipyard"
    };
    app->showcase_ship_idx = (app->showcase_ship_idx + 1) % 5;
    const char *bp_id = showcase_bps[app->showcase_ship_idx];
    uint32_t eid = find_entity_by_blueprint(&app->world, bp_id, 1u);
    if (eid == 0u && strcmp(bp_id, "ship_fleet_carrier") == 0) {
        eid = rts_world_spawn_from_blueprint(&app->world, "ship_fleet_carrier", "Fleet Carrier Helios", 1u,
                                             app->world.planet_frame_id,
                                             rts_v3d(180.0, app->world.planet.radius_m, -140.0), true);
    }
    if (eid != 0u) {
        sandbox_enter_entity_mega(app, eid, 0u);
    }
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
    rts_crew_init(&app->crew);
    rts_hologlobe_init(&app->holo);
    rts_field_gear_init(&app->gear);
    rts_combat_init(&app->combat);
    rts_vehicle_physics_init(&app->vphys);
    RtsFeelConfig feel = cfg->fun_baseline ? rts_feel_config_baseline() : rts_feel_config_solaris();
    rts_tactical_init(&app->tactical, &feel, seed, &app->audio);

    if (!rts_renderer_init(&app->renderer, rts_gfx_gl_create(), &app->vfs, cfg->width, cfg->height)) {
        fprintf(stderr, "[platform_sdl] Renderer init failed\n");
        return false;
    }
    rts_renderer_set_rts_toolkit(&app->renderer, &app->holo, &app->gear, &app->economy);
    rts_renderer_set_live_combat_physics(&app->renderer, &app->combat, &app->vphys);
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
    /* Hydrate starting Citadel as a fully staffed 72-room / 6-deck explorable mega-interior */
    app->showcase_ship_idx = 0;
    if (!sandbox_enter_entity_mega(app, app->world.commander.occupied_entity_id, 0u)) {
        app->world.commander.local_pos = rts_v3d(0.0, 1.75, 0.2);
        app->world.commander.yaw_rad = 0.0;
        app->world.commander.pitch_rad = -0.05;
        rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS", "",
                                     "WALK WAR ROOM [WASD] | [TAB] WRIST-PAD | [Z] OPTICS | [V] RADIAL | [E] HOLO-GLOBE");
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

static void sandbox_trigger_astrogator(InteractiveApp *app) {
    uint32_t dropship_id = find_entity_by_blueprint(&app->world, "air_dropship", 1u);
    uint32_t shipyard_id = find_entity_by_blueprint(&app->world, "stn_orbital_shipyard", 1u);
    RtsEntity *ds = rts_world_get_entity(&app->world, dropship_id);
    if (!ds) return;

    /* Board Commander into Dropship Valkyrie-2 with its full 3-room staffed interior */
    if (ds->own_frame_id != RTS_FRAME_INVALID) {
        sandbox_enter_entity_mega(app, ds->id, 0u);
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
            rts_renderer_set_active_interior(&app->renderer, NULL, NULL);
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
        sandbox_enter_entity_mega(app, tgt->id, 0u);
    }
}

static RtsRay sandbox_commander_ray(const InteractiveApp *app, bool world_space) {
    double yaw = app->world.commander.yaw_rad;
    double pitch = app->world.commander.pitch_rad;
    RtsVec3d dir = rts_v3d_norm(rts_v3d(sin(yaw) * cos(pitch), sin(pitch), -cos(yaw) * cos(pitch)));
    if (!world_space) {
        return (RtsRay){app->world.commander.local_pos, dir};
    }
    RtsVec3d cam_w = rts_v3d(0.0, app->world.planet.radius_m, 0.0);
    rts_interior_local_to_world(&app->world.interiors, app->world.commander.frame_id, app->world.commander.local_pos, &cam_w);
    return (RtsRay){cam_w, dir};
}

static const RtsConsoleStation *sandbox_find_active_hologlobe(const InteractiveApp *app) {
    for (size_t c = 0; c < app->world.interiors.console_count; ++c) {
        const RtsConsoleStation *cs = &app->world.interiors.consoles[c];
        if (cs->active && cs->frame_id == app->world.commander.frame_id && cs->type == RTS_CONSOLE_HOLO_GLOBE) {
            return cs;
        }
    }
    return NULL;
}

static void sandbox_hologlobe_update_hover(InteractiveApp *app) {
    const RtsConsoleStation *hg_cs = sandbox_find_active_hologlobe(app);
    if (!hg_cs) return;
    RtsVec3d gcenter = rts_v3d(hg_cs->local_pos.x, hg_cs->local_pos.y + 0.55, hg_cs->local_pos.z);
    RtsRay ray = sandbox_commander_ray(app, false);
    if (rts_hologlobe_raycast(&app->holo, &app->world, 1u, ray, gcenter, 0.92)) {
        if (app->holo.mode == RTS_HOLO_MODE_PLACE_BUILDING) {
            rts_hologlobe_validate_placement(&app->holo, &app->world, &app->economy, 1u,
                                             app->holo.place_blueprint_id, app->holo.hovered_world_pos);
        }
    }
}

static void sandbox_hologlobe_click(InteractiveApp *app, bool additive) {
    sandbox_hologlobe_update_hover(app);
    if (app->holo.mode == RTS_HOLO_MODE_PLACE_BUILDING) {
        uint32_t id = rts_hologlobe_confirm_placement(&app->holo, &app->world, &app->economy, 1u,
                                                      app->holo.place_blueprint_id, app->holo.ghost_world_pos);
        if (id > 0u) {
            rts_renderer_set_live_status(&app->renderer, "3D HOLO-GLOBE DEPLOYMENT", "", app->holo.last_feedback);
        } else {
            rts_renderer_set_live_status(&app->renderer, "3D PLACEMENT VALIDATION", "", app->holo.last_feedback);
        }
        return;
    }
    if (app->holo.mode == RTS_HOLO_MODE_SELECT) {
        if (app->holo.hovered_entity_id != 0u) {
            rts_hologlobe_select_entity(&app->holo, app->holo.hovered_entity_id, additive);
            const RtsEntity *e = rts_world_get_entity_const(&app->world, app->holo.hovered_entity_id);
            snprintf(app->holo.last_feedback, sizeof(app->holo.last_feedback),
                     "SELECTED %.44s (%zu TOTAL SELECTED)", e ? e->name : "UNIT", app->holo.selected_count);
            rts_renderer_set_live_status(&app->renderer, "3D HOLO-GLOBE SELECT", "", app->holo.last_feedback);
        }
        return;
    }
    if (app->holo.hovered_valid) {
        rts_hologlobe_issue_order(&app->holo, &app->world, &app->ctrl, &app->boarding, 1u,
                                  app->holo.mode, app->holo.hovered_world_pos, app->holo.hovered_entity_id);
        rts_renderer_set_live_status(&app->renderer, "3D HOLO-GLOBE COMMAND", "", app->holo.last_feedback);
    }
}

static void interactive_tick(void *arg) {
    InteractiveApp *app = (InteractiveApp *)arg;
    rts_input_begin_frame(&app->input);

    int web_touch = rts_web_is_touch_device();
    if (web_touch != app->last_web_touch_state) {
        rts_input_set_mode_override(&app->input, web_touch != 0,
                                    web_touch ? RTS_INPUT_MOBILE_TOUCH : RTS_INPUT_DESKTOP_KBM);
        app->last_web_touch_state = web_touch;
    }

    RtsTacticalInput tin;
    memset(&tin, 0, sizeof(tin));
    tin.aim_yaw = app->tactical.player_yaw;
    tin.aim_pitch = app->tactical.player_pitch;

    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_EVENT_QUIT) {
            app->running = false;
        } else if (ev.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            int nw = ev.window.data1;
            int nh = ev.window.data2;
            if (nw > 64 && nh > 64) {
                app->renderer.width = nw;
                app->renderer.height = nh;
                rts_input_resize(&app->input, (float)nw, (float)nh);
            }
        } else if (ev.type == SDL_EVENT_MOUSE_WHEEL && app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
            rts_hologlobe_zoom(&app->holo, (double)ev.wheel.y * 0.25);
        } else if (ev.type == SDL_EVENT_KEY_DOWN && !ev.key.repeat) {
            rts_input_notify_kbm_activity(&app->input);
#ifndef __EMSCRIPTEN__
            if (ev.key.key == SDLK_ESCAPE) app->running = false;
#endif
            if (ev.key.key == SDLK_L) {
                app->view_mode = (app->view_mode == RTS_VIEW_TACTICAL) ? RTS_VIEW_SANDBOX_RTS : RTS_VIEW_TACTICAL;
            }
            if (ev.key.key == SDLK_M) {
                app->view_mode = RTS_VIEW_SANDBOX_RTS;
                rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS", "",
                                             "[TAB] WRIST-PAD | [Z] OPTICS | [V] RADIAL | [E] HOLO-GLOBE/CONSOLE | [/] LIFT | [N] SHIP");
            }
            if (ev.key.key == SDLK_C) {
                if (app->view_mode == RTS_VIEW_GALLERY) {
                    app->gallery_cls = (RtsConstructClass)(((int)app->gallery_cls + 1) % 6);
                } else {
                    app->view_mode = RTS_VIEW_GALLERY;
                    app->gallery_cls = RTS_CLASS_BUILDING;
                }
            }
            if (ev.key.key == SDLK_N) {
                app->view_mode = RTS_VIEW_SANDBOX_RTS;
                rts_facility_close(&app->gear);
                sandbox_cycle_megaship(app);
            }
            if (ev.key.key == SDLK_LEFTBRACKET && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                if (app->gear.wrist_pad_open) {
                    rts_field_gear_cycle_wrist_tab(&app->gear, -1);
                } else {
                    sandbox_switch_deck(app, -1);
                }
            }
            if (ev.key.key == SDLK_RIGHTBRACKET && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                if (app->gear.wrist_pad_open) {
                    rts_field_gear_cycle_wrist_tab(&app->gear, +1);
                } else {
                    sandbox_switch_deck(app, +1);
                }
            }
            if (ev.key.key == SDLK_TAB) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                    rts_field_gear_toggle_wrist_pad(&app->gear, &app->world);
                    rts_renderer_set_live_status(&app->renderer, "TACTICAL WRIST-PAD", "", app->gear.last_status);
                } else {
                    app->input.intent.toggle_pad_pressed = true;
                }
            }
            if (ev.key.key == SDLK_Z && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                rts_field_gear_cycle_optics(&app->gear);
                rts_renderer_set_live_status(&app->renderer, "OPTICAL LASER DESIGNATOR", "", app->gear.last_status);
            }
            if (ev.key.key == SDLK_V && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                rts_field_gear_toggle_radial(&app->gear);
                rts_renderer_set_live_status(&app->renderer, "RADIAL COMMAND WHEEL", "", app->gear.last_status);
            }
            if (ev.key.key == SDLK_X && app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
                rts_hologlobe_cycle_building(&app->holo, &app->world, +1);
                sandbox_hologlobe_update_hover(app);
                rts_renderer_set_live_status(&app->renderer, "3D BASE PLACEMENT", "", app->holo.last_feedback);
            }
            if (ev.key.key == SDLK_E) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->gear.facility_open) {
                    rts_facility_close(&app->gear);
                    app->ctrl.mode = RTS_CMD_ON_FOOT;
                    rts_renderer_set_live_status(&app->renderer, "FACILITY TERMINAL CLOSED", "", "STEPPED BACK FROM PRODUCTION CONSOLE.");
                } else {
                    const RtsConsoleStation *near_c = (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->ctrl.mode == RTS_CMD_ON_FOOT)
                        ? rts_interior_find_nearest_console(&app->world.interiors, app->world.commander.frame_id, app->world.commander.local_pos)
                        : NULL;
                    if (near_c && near_c->type == RTS_CONSOLE_TURBOLIFT) {
                        sandbox_switch_deck(app, +1);
                    } else if (near_c && near_c->type == RTS_CONSOLE_FACTORY_TERMINAL) {
                        if (rts_facility_open_for_entity(&app->gear, &app->world, near_c->owner_entity_id)) {
                            app->ctrl.mode = RTS_CMD_AT_FACTORY;
                            rts_renderer_set_live_status(&app->renderer, "TIER 2 PRODUCTION TERMINAL", "", app->gear.last_status);
                        }
                    } else {
                        app->input.intent.interact_pressed = true;
                    }
                }
            }
            if (ev.key.key == SDLK_G && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                sandbox_trigger_astrogator(app);
            }
            if (ev.key.key == SDLK_B && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                rts_facility_close(&app->gear);
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
            if (ev.key.key == SDLK_Q) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
                    rts_hologlobe_zoom(&app->holo, -0.35);
                } else {
                    tin.action_pressed[RTS_ACT_EMP_PULSE] = true;
                }
            }
            if (ev.key.key == SDLK_LSHIFT || ev.key.key == SDLK_RSHIFT) tin.action_pressed[RTS_ACT_THRUSTER_DASH] = true;
            if (ev.key.key == SDLK_SPACE) tin.jump_pressed = true;
            if (ev.key.key == SDLK_F) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->mega.total_rooms > 0u) {
                    bool next_alert = !app->crew.red_alert;
                    rts_crew_set_red_alert(&app->crew, &app->mega, &app->world, next_alert);
                    rts_renderer_set_live_status(&app->renderer,
                                                 next_alert ? "RED ALERT - BOARDERS & DAMAGE CONTROL" : "ALL CLEAR - STANDARD OPS", "",
                                                 next_alert ? "RED ALERT! ENGINEERS SPRINTING TO REPAIR DAMAGED CORES | SOLAR MARINES INTERCEPTING BREACHERS!"
                                                            : "RED ALERT STOOD DOWN - CREW RETURNING TO STATIONS & PATROL.");
                } else {
                    tin.action_pressed[RTS_ACT_FLEET_STRIKE] = true;
                }
            }
            if (ev.key.key == SDLK_PERIOD && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                bool fighting = rts_combat_is_active(&app->combat, &app->world);
                int f = rts_time_compression_cycle(&app->vphys, fighting);
                char tw[128];
                snprintf(tw, sizeof(tw), "TIME COMPRESSION %dX%s", f, fighting ? " (COMBAT ACTIVE - LOCKED AT 1X)" : " ACTIVE");
                rts_renderer_set_live_status(&app->renderer, "TIME COMPRESSION", "", tw);
            }
            if (ev.key.key == SDLK_K && app->view_mode == RTS_VIEW_SANDBOX_RTS) {
                uint32_t ds_id = find_entity_by_blueprint(&app->world, "air_dropship", 1u);
                uint32_t tk_id = find_entity_by_blueprint(&app->world, "veh_plasma_tank", 1u);
                if (app->vphys.clamped_cargo_id != 0u) {
                    if (rts_vehicle_release_cargo(&app->vphys, &app->world)) {
                        rts_renderer_set_live_status(&app->renderer, "DROPSHIP CARGO BAY", "", "RELEASED CLAMPED GROUND VEHICLE ONTO SURFACE!");
                    }
                } else if (ds_id != 0u && tk_id != 0u) {
                    rts_vehicle_physics_bind(&app->vphys, &app->world, ds_id);
                    if (rts_vehicle_clamp_cargo(&app->vphys, &app->world, ds_id, tk_id)) {
                        rts_renderer_set_live_status(&app->renderer, "DROPSHIP CARGO BAY", "", "CLAMPED PLASMA TANK INSIDE DROPSHIP BAY FOR ORBITAL LIFT!");
                    }
                }
            }
            if (ev.key.key == SDLK_H) tin.action_pressed[RTS_ACT_FIELD_REPAIR] = true;
            if (ev.key.key == SDLK_R || ev.key.key == SDLK_RETURN) {
                if (ev.key.key == SDLK_R && app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
                    rts_hologlobe_zoom(&app->holo, +0.35);
                } else {
                    tin.retry_pressed = true;
                }
            }

            /* Context-Aware Number Keys [1-9]: Radial Wheel -> Facility Console -> Tier 3 Holo-Globe -> Wrist-Pad -> Showcase */
            int num = -1;
            if (ev.key.key >= SDLK_1 && ev.key.key <= SDLK_9) num = (int)(ev.key.key - SDLK_1);
            if (num >= 0) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->gear.radial_open && num < RTS_RADIAL_COUNT) {
                    RtsVec3d cw = rts_v3d(0.0, app->world.planet.radius_m, 0.0);
                    rts_interior_local_to_world(&app->world.interiors, app->world.commander.frame_id, app->world.commander.local_pos, &cw);
                    rts_field_gear_exec_radial(&app->gear, &app->world, &app->ctrl, &app->boarding, &app->holo, 1u, (RtsRadialOrder)num, cw);
                    rts_renderer_set_live_status(&app->renderer, "RADIAL QUICK ORDER", "", app->gear.last_status);
                } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->gear.facility_open && num < 6) {
                    rts_facility_queue_index(&app->gear, &app->economy, &app->world, num);
                    rts_renderer_set_live_status(&app->renderer, "1:1 GANTRY ASSEMBLY", "", app->gear.last_status);
                } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe && num < 7) {
                    static const RtsHoloOrderMode modes[7] = {
                        RTS_HOLO_MODE_SELECT, RTS_HOLO_MODE_MOVE, RTS_HOLO_MODE_ATTACK,
                        RTS_HOLO_MODE_ORBIT, RTS_HOLO_MODE_DOCK, RTS_HOLO_MODE_REENTRY, RTS_HOLO_MODE_BOARD
                    };
                    app->holo.mode = modes[num];
                    snprintf(app->holo.last_feedback, sizeof(app->holo.last_feedback),
                             "HOLO-GLOBE MODE: %s - AIM & CLICK LMB ON GLOBE", rts_hologlobe_mode_name(app->holo.mode));
                    rts_renderer_set_live_status(&app->renderer, "3D HOLO-GLOBE", "", app->holo.last_feedback);
                } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->gear.wrist_pad_open && num < RTS_WPAD_TAB_COUNT) {
                    app->gear.wrist_tab = (RtsWristPadTab)num;
                    rts_renderer_set_live_status(&app->renderer, "TACTICAL WRIST-PAD", "", rts_wrist_tab_name(app->gear.wrist_tab));
                } else {
                    app->view_mode = RTS_VIEW_SHOWCASE;
                    app->scene = (RtsShowcaseScene)num;
                }
            }
        } else if (ev.type == SDL_EVENT_MOUSE_MOTION) {
            if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe &&
                (ev.motion.state & SDL_BUTTON_RMASK)) {
                rts_hologlobe_orbit_drag(&app->holo, ev.motion.xrel * 0.008, ev.motion.yrel * 0.008);
            } else {
                tin.aim_yaw += ev.motion.xrel * 0.0045f;
                tin.aim_pitch = rts_clampf(tin.aim_pitch - ev.motion.yrel * 0.0045f, -1.1f, 1.1f);
                app->input.intent.look_dx += ev.motion.xrel * 0.0045f;
                app->input.intent.look_dy -= ev.motion.yrel * 0.0045f;
            }
        } else if (ev.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
            if (ev.button.button == SDL_BUTTON_LEFT) {
                if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->world.commander.at_holo_globe) {
                    const bool *ks = SDL_GetKeyboardState(NULL);
                    bool add = ks && (ks[SDL_SCANCODE_LSHIFT] || ks[SDL_SCANCODE_RSHIFT]);
                    sandbox_hologlobe_click(app, add);
                } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->gear.optics_level > 0) {
                    RtsRay wray = sandbox_commander_ray(app, true);
                    if (rts_field_gear_paint_target(&app->gear, &app->world, 1u, wray)) {
                        rts_field_gear_designate_strike(&app->gear, &app->world, 1u);
                        rts_renderer_set_live_status(&app->renderer, "LASER DESIGNATOR", "", app->gear.last_status);
                    }
                } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS && app->ctrl.mode == RTS_CMD_PILOTING && app->ctrl.piloted_entity_id != 0u) {
                    RtsRay wray = sandbox_commander_ray(app, true);
                    const RtsEntity *pe = rts_world_get_entity_const(&app->world, app->ctrl.piloted_entity_id);
                    if (pe) {
                        RtsVec3d aim_pt = rts_v3d_add(wray.origin, rts_v3d_scale(wray.dir, 180.0));
                        rts_combat_fire(&app->combat, &app->world, pe->id, 0u, aim_pt, rts_combat_weapon_for_entity(pe));
                    }
                } else {
                    tin.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
                }
            } else if (ev.button.button == SDL_BUTTON_RIGHT && app->view_mode == RTS_VIEW_SANDBOX_RTS && !app->world.commander.at_holo_globe) {
                rts_field_gear_toggle_radial(&app->gear);
                rts_renderer_set_live_status(&app->renderer, "RADIAL COMMAND WHEEL", "", app->gear.last_status);
            }
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
        if (keys[SDL_SCANCODE_SPACE]) { app->input.intent.jump_pressed = true; }
        if (keys[SDL_SCANCODE_LCTRL]) { app->input.intent.crouch_pressed = true; }
        if (keys[SDL_SCANCODE_LEFT])  { tin.aim_yaw -= 0.04f; app->input.intent.look_dx -= 0.04f; }
        if (keys[SDL_SCANCODE_RIGHT]) { tin.aim_yaw += 0.04f; app->input.intent.look_dx += 0.04f; }
    }

    /* Forward mobile touch virtual controls seamlessly into live tactical defense combat */
    if (app->input.mode == RTS_INPUT_MOBILE_TOUCH) {
        if (app->input.stick_active) {
            tin.move_x = app->input.intent.move_x;
            tin.move_z = app->input.intent.move_y;
        }
        tin.aim_yaw += app->input.intent.look_dx * 0.05f;
        tin.aim_pitch = rts_clampf(tin.aim_pitch + app->input.intent.look_dy * 0.05f, -1.1f, 1.1f);
        if (app->input.intent.interact_pressed || app->input.intent.designate_held) {
            tin.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
        }
        if (app->input.intent.toggle_pad_pressed) {
            tin.action_pressed[RTS_ACT_EMP_PULSE] = true;
        }
        if (app->input.intent.mag_boot_pressed) {
            tin.action_pressed[RTS_ACT_THRUSTER_DASH] = true;
        }
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
    } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS) {
        bool fighting = rts_combat_is_active(&app->combat, &app->world);
        int warp = rts_time_compression_set(&app->vphys, app->vphys.time_compression, fighting);
        for (int ws = 0; ws < warp; ++ws) {
            if (app->ctrl.mode == RTS_CMD_PILOTING && app->ctrl.piloted_entity_id != 0u) {
                if (app->vphys.entity_id != app->ctrl.piloted_entity_id) {
                    rts_vehicle_physics_bind(&app->vphys, &app->world, app->ctrl.piloted_entity_id);
                }
                rts_vehicle_physics_step(&app->vphys, &app->world, &app->input.intent, 1.0 / 60.0);
            } else {
                rts_controller_step(&app->ctrl, &app->world, &app->input.intent, 1.0 / 60.0);
            }
            if (app->world.commander.at_holo_globe) {
                sandbox_hologlobe_update_hover(app);
            }
            if (app->mega.total_rooms > 0u && app->world.commander.occupied_entity_id == app->mega.entity_id) {
                rts_crew_step(&app->crew, &app->mega, &app->world, 1.0 / 60.0);
            }
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
            rts_combat_step(&app->combat, &app->world, (app->mega.total_rooms > 0u) ? &app->mega : NULL, 1.0 / 60.0);
            rts_economy_step(&app->economy, &app->world, 1.0 / 60.0);
            rts_boarding_step(&app->boarding, &app->world, 1.0 / 60.0);
            rts_world_step(&app->world, 1.0 / 60.0);
        }
    } else {
        rts_economy_step(&app->economy, &app->world, 1.0 / 60.0);
        rts_boarding_step(&app->boarding, &app->world, 1.0 / 60.0);
        rts_world_step(&app->world, 1.0 / 60.0);
    }

    if (xr_on) {
        rts_renderer_render_webxr_stereo(&app->renderer, &app->world, &app->input, &xr, app->time_s);
    } else if (app->view_mode == RTS_VIEW_TACTICAL) {
        rts_renderer_render_tactical(&app->renderer, &app->tactical, &app->world, &app->input, app->time_s);
    } else if (app->view_mode == RTS_VIEW_SANDBOX_RTS) {
        rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, app->time_s);
    } else if (app->view_mode == RTS_VIEW_GALLERY) {
        rts_renderer_render_roster_class(&app->renderer, &app->world, &app->input, app->gallery_cls, app->time_s);
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
    app->last_web_touch_state = -1;
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
    emscripten_set_main_loop_arg(interactive_tick, app, 0, 0);
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

    /* Also capture live Seamless Sandbox RTS Mode inside the hydrated 72-room Citadel War Room with Holo-Globe active */
    app->world.commander.local_pos.y = 2.0;
    app->world.commander.yaw_rad = 0.04;
    app->world.commander.pitch_rad = -0.05;
    app->world.commander.at_holo_globe = true;
    rts_renderer_set_live_status(&app->renderer, "SEAMLESS SANDBOX RTS [MODE M]", "",
                                 "LIVE 72-ROOM CITADEL WAR ROOM: [1] QUEUE TANK | [2] ATTACK | [3] 400KM ORBIT | [4] BOARDING | [/] TURBOLIFT");
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

static bool save_live_interior_shot(InteractiveApp *app, const char *dir, const char *filename, float sim_t) {
    rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, sim_t);
    SDL_GL_SwapWindow(app->window);
    rts_renderer_render_live_world(&app->renderer, &app->world, &app->input, sim_t);
    char out_path[512];
    snprintf(out_path, sizeof(out_path), "%s/%s", dir, filename);
    bool ok = rts_renderer_capture_png(&app->renderer, out_path);
    SDL_GL_SwapWindow(app->window);
    if (ok) {
        printf("[mega_interiors] Saved %s\n", out_path);
    } else {
        fprintf(stderr, "[mega_interiors] Failed to write %s\n", out_path);
    }
    return ok;
}

static const RtsInteriorRoom *find_hydrated_room_kind(const RtsWorld *w, RtsFrameId fid, RtsRoomKind kind) {
    for (size_t i = 0; i < w->interiors.room_count; ++i) {
        const RtsInteriorRoom *rm = &w->interiors.rooms[i];
        if (rm->active && rm->frame_id == fid && rm->kind == kind) return rm;
    }
    return NULL;
}

int rts_platform_capture_mega_interiors(const RtsAppConfig *cfg) {
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

    /* Shot 1: 420-Room / 20-Deck Fleet Command Dreadnought Deck 1 Bridge & War Room (Fully Staffed) */
    uint32_t dread_id = find_entity_by_blueprint(&app->world, "space_command_dreadnought", 1u);
    if (dread_id != 0u && sandbox_enter_entity_mega(app, dread_id, 0u)) {
        const RtsInteriorRoom *war = find_hydrated_room_kind(&app->world, app->world.commander.frame_id, RTS_ROOM_WAR_ROOM);
        if (war) {
            double cx = 0.5 * (war->local_bounds.min.x + war->local_bounds.max.x);
            double cz = 0.5 * (war->local_bounds.min.z + war->local_bounds.max.z);
            app->world.commander.local_pos = rts_v3d(cx, 2.05, cz + 3.8);
            app->world.commander.yaw_rad = 0.0;
            app->world.commander.pitch_rad = -0.07;
        }
        rts_renderer_set_live_status(&app->renderer, "DREADNOUGHT SOLARIS (420 ROOMS / 20 DECKS)", "",
                                     "DECK 1 WAR ROOM & BRIDGE: 3D HOLO-GLOBE, STAFFED COMMAND OFFICERS & PANORAMIC 400KM LEO VIEWPORT");
        if (!save_live_interior_shot(app, cfg->mega_interiors_dir, "interior_01_dreadnought_bridge_staffed.png", 1.4f)) rc = 1;

        /* Shot 2: Walking Through Open Bulkhead Doorways Down Main Spine Corridor & Turbolift Hub with Patrolling Marines */
        for (int t = 0; t < 75; ++t) {
            rts_crew_step(&app->crew, &app->mega, &app->world, 1.0 / 60.0);
        }
        app->world.commander.local_pos = rts_v3d(-13.6, 1.88, 0.55);
        app->world.commander.yaw_rad = 1.54;
        app->world.commander.pitch_rad = -0.04;
        rts_renderer_set_live_status(&app->renderer, "DREADNOUGHT SPINE CORRIDOR & TURBOLIFT", "",
                                     "SEAMLESS MULTI-ROOM DOORWAYS: PATROLLING SOLAR MARINES, TURBOLIFT SHAFT & LIVE DECK BLUEPRINT MAP");
        if (!save_live_interior_shot(app, cfg->mega_interiors_dir, "interior_02_dreadnought_corridor_marines.png", 2.6f)) rc = 1;
    } else {
        rc = 1;
    }

    /* Shot 3: 420-Room / 20-Deck Orbital Shipyard Reactor Core Deck under Red Alert with Engineers Weld-Repairing */
    uint32_t ship_id = find_entity_by_blueprint(&app->world, "stn_orbital_shipyard", 1u);
    if (ship_id != 0u && sandbox_enter_entity_mega(app, ship_id, 0u)) {
        uint16_t rdeck = rts_mega_find_deck_with_kind(&app->mega, RTS_ROOM_REACTOR);
        sandbox_enter_entity_mega(app, ship_id, rdeck);
        rts_crew_set_red_alert(&app->crew, &app->mega, &app->world, true);
        for (int t = 0; t < 105; ++t) {
            rts_crew_step(&app->crew, &app->mega, &app->world, 1.0 / 60.0);
        }
        const RtsInteriorRoom *rrm = find_hydrated_room_kind(&app->world, app->world.commander.frame_id, RTS_ROOM_REACTOR);
        if (rrm) {
            double cx = 0.5 * (rrm->local_bounds.min.x + rrm->local_bounds.max.x);
            double cz = 0.5 * (rrm->local_bounds.min.z + rrm->local_bounds.max.z);
            app->world.commander.local_pos = rts_v3d(cx - 1.5, 2.0, cz + 4.1);
            app->world.commander.yaw_rad = 0.30;
            app->world.commander.pitch_rad = -0.05;
        }
        rts_renderer_set_live_status(&app->renderer, "ORBITAL SHIPYARD RED ALERT - REACTOR DECK", "",
                                     "420-ROOM STATION DECK 14 REACTOR CORE: TOKAMAK CONTAINMENT & ENGINEERS EXECUTING LIVE DAMAGE CONTROL!");
        if (!save_live_interior_shot(app, cfg->mega_interiors_dir, "interior_03_shipyard_reactor_deck.png", 3.8f)) rc = 1;
    } else {
        rc = 1;
    }

    /* Shot 4: 150-Room / 10-Deck Fleet Starfighter Carrier Hangar Deck with Parked Starfighters & Flight Crew */
    uint32_t car_id = find_entity_by_blueprint(&app->world, "ship_fleet_carrier", 1u);
    if (car_id == 0u) {
        car_id = rts_world_spawn_from_blueprint(&app->world, "ship_fleet_carrier", "Fleet Carrier Helios", 1u,
                                                app->world.planet_frame_id,
                                                rts_v3d(180.0, app->world.planet.radius_m, -140.0), true);
    }
    if (car_id != 0u && sandbox_enter_entity_mega(app, car_id, 0u)) {
        uint16_t hdeck = rts_mega_find_deck_with_kind(&app->mega, RTS_ROOM_HANGAR);
        sandbox_enter_entity_mega(app, car_id, hdeck);
        const RtsInteriorRoom *hrm = find_hydrated_room_kind(&app->world, app->world.commander.frame_id, RTS_ROOM_HANGAR);
        if (hrm) {
            double cx = 0.5 * (hrm->local_bounds.min.x + hrm->local_bounds.max.x);
            double cz = 0.5 * (hrm->local_bounds.min.z + hrm->local_bounds.max.z);
            app->world.commander.local_pos = rts_v3d(cx + 3.4, 2.22, cz + 3.9);
            app->world.commander.yaw_rad = -0.58;
            app->world.commander.pitch_rad = -0.14;
        }
        rts_renderer_set_live_status(&app->renderer, "FLEET CARRIER HELIOS (150 ROOMS / 10 DECKS)", "",
                                     "DECK 2 STARFIGHTER HANGAR BAY: SCULPTED STARFIGHTERS, FLIGHT CREW, PILOTS & LAUNCH TERMINALS");
        if (!save_live_interior_shot(app, cfg->mega_interiors_dir, "interior_04_carrier_hangar_deck.png", 5.0f)) rc = 1;
    } else {
        rc = 1;
    }

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[mega_interiors] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) {
        fprintf(stderr, "[mega_interiors] FAIL: OpenGL reported %u errors\n", st.gl_errors);
        rc = 1;
    }

    shutdown_engine_stack(app);
    free(app);
    return rc;
}

int rts_platform_capture_rts_toolkit(const RtsAppConfig *cfg) {
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

    /* Shot 1: Tier 3 Interactive 3D Planetary & Orbital Holo-Globe with Raycast Selection, Order Arc & 3D Building Ghost */
    uint32_t tank_id = find_entity_by_blueprint(&app->world, "veh_plasma_tank", 1u);
    uint32_t enemy_id = find_entity_by_blueprint(&app->world, "veh_plasma_tank", 2u);
    uint32_t ds_id = find_entity_by_blueprint(&app->world, "air_dropship", 1u);
    if (tank_id != 0u && enemy_id != 0u) {
        const RtsEntity *enemy = rts_world_get_entity_const(&app->world, enemy_id);
        rts_hologlobe_select_entity(&app->holo, tank_id, false);
        if (ds_id != 0u) rts_hologlobe_select_entity(&app->holo, ds_id, true);
        if (enemy) {
            rts_world_issue_order(&app->world, tank_id, RTS_ORDER_ATTACK, enemy->pos, enemy_id, 0.0);
        }
    }
    app->holo.mode = RTS_HOLO_MODE_PLACE_BUILDING;
    snprintf(app->holo.place_blueprint_id, sizeof(app->holo.place_blueprint_id), "bldg_plasma_turret");
    rts_hologlobe_validate_placement(&app->holo, &app->world, &app->economy, 1u, "bldg_plasma_turret",
                                     rts_v3d(-85.0, app->world.planet.radius_m, 45.0));
    app->world.commander.at_holo_globe = true;
    app->world.commander.local_pos.y = 2.02;
    app->world.commander.yaw_rad = 0.0;
    app->world.commander.pitch_rad = -0.06;
    rts_renderer_set_live_status(&app->renderer, "TIER 3 INTERACTIVE 3D HOLO-GLOBE", "",
                                 "LIVE 3D GLOBE: RAYCAST SELECTION HALOS, ATTACK TRAJECTORY, KEPLERIAN TRACKS & 3D BUILDING GHOST");
    if (!save_live_interior_shot(app, cfg->rts_toolkit_dir, "toolkit_01_hologlobe_raycast_placement.png", 1.8f)) rc = 1;

    /* Shot 2: Tier 1 Diegetic 4-Tab Tactical Wrist-Pad (Spherical-Horizon Fog-of-War Radar + Comms Link) */
    app->world.commander.at_holo_globe = false;
    app->world.commander.frame_id = app->world.planet_frame_id;
    app->world.commander.occupied_entity_id = 0u;
    app->world.commander.local_pos = rts_v3d(42.0, app->world.planet.radius_m + 3.2, -16.0);
    app->world.commander.yaw_rad = -0.78;
    app->world.commander.pitch_rad = -0.04;
    rts_renderer_set_active_interior(&app->renderer, NULL, NULL);
    app->gear.wrist_pad_open = true;
    app->gear.wrist_tab = RTS_WPAD_RADAR;
    rts_renderer_set_live_status(&app->renderer, "TIER 1 DIEGETIC TACTICAL WRIST-PAD", "",
                                 "4-TAB WRIST-PAD [TAB]: SPHERICAL-HORIZON FOG-OF-WAR RADAR, COMMS RELAY STATUS, FLEET ROSTER & ECONOMY");
    if (!save_live_interior_shot(app, cfg->rts_toolkit_dir, "toolkit_02_wristpad_tactical_radar.png", 2.9f)) rc = 1;

    /* Shot 3: Tier 1 Optical Binoculars (4x Zoom) + 3D Laser Designator Beacon + 7-Order Radial Quick Wheel */
    app->gear.wrist_pad_open = false;
    app->gear.optics_level = 1; /* 4x optical zoom (20 deg FOV) */
    const RtsEntity *raider = rts_world_get_entity_const(&app->world, enemy_id);
    if (raider) {
        app->world.commander.local_pos = rts_v3d(raider->pos.x - 105.0, app->world.planet.radius_m + 4.5, raider->pos.z + 24.0);
        RtsVec3d to_r = rts_v3d_sub(raider->pos, app->world.commander.local_pos);
        app->world.commander.yaw_rad = atan2(to_r.x, -to_r.z);
        app->world.commander.pitch_rad = -0.02;
        RtsRay lray = {app->world.commander.local_pos, rts_v3d_norm(to_r)};
        rts_field_gear_paint_target(&app->gear, &app->world, 1u, lray);
        rts_field_gear_designate_strike(&app->gear, &app->world, 1u);
    }
    app->gear.radial_open = true;
    app->gear.radial_hovered = RTS_RADIAL_ATTACK;
    rts_renderer_set_live_status(&app->renderer, "TIER 1 OPTICS 4X & RADIAL WHEEL", "",
                                 "OPTICAL BINOCULARS [Z] PAINTING 3D LASER TARGET ON CRIMSON RAIDER 1 + 7-ORDER RADIAL QUICK WHEEL [V]");
    if (!save_live_interior_shot(app, cfg->rts_toolkit_dir, "toolkit_03_optics_laser_radial.png", 4.1f)) rc = 1;

    /* Shot 4: Tier 2 Interactive Foundry Build Terminal + Live 1:1 Progressive Gantry Assembly */
    app->gear.optics_level = 0;
    app->gear.radial_open = false;
    app->gear.paint_active = false;
    uint32_t foundry_id = find_entity_by_blueprint(&app->world, "bldg_vehicle_foundry", 1u);
    const RtsEntity *foundry = rts_world_get_entity_const(&app->world, foundry_id);
    if (foundry) {
        rts_facility_open_for_entity(&app->gear, &app->world, foundry_id);
        uint32_t jid = rts_facility_queue_index(&app->gear, &app->economy, &app->world, 0);
        if (jid > 0u) {
            /* Advance build to ~62% completion (3.1s of 5.0s) so rising solid tank hull + holographic scaffold + golden weld beams shine */
            for (int t = 0; t < 93; ++t) {
                rts_economy_step(&app->economy, &app->world, 1.0 / 30.0);
            }
        }
        RtsVec3d gantry = rts_v3d(foundry->pos.x + 28.0, foundry->pos.y + 1.8, foundry->pos.z + 32.0);
        app->world.commander.local_pos = rts_v3d(gantry.x + 14.0, foundry->pos.y + 4.8, gantry.z + 18.0);
        RtsVec3d look = rts_v3d_sub(gantry, app->world.commander.local_pos);
        app->world.commander.yaw_rad = atan2(look.x, -look.z);
        app->world.commander.pitch_rad = -0.10;
    }
    rts_renderer_set_live_status(&app->renderer, "TIER 2 FOUNDRY 1:1 GANTRY ASSEMBLY", "",
                                 "SOLARIS VEHICLE FOUNDRY TERMINAL [E]: PLASMA HEAVY TANK ASSEMBLING PROGRESSIVELY AT 1:1 SCALE!");
    if (!save_live_interior_shot(app, cfg->rts_toolkit_dir, "toolkit_04_factory_1to1_assembly.png", 5.3f)) rc = 1;

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[rts_toolkit] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) {
        fprintf(stderr, "[rts_toolkit] FAIL: OpenGL reported %u errors\n", st.gl_errors);
        rc = 1;
    }

    shutdown_engine_stack(app);
    free(app);
    return rc;
}

int rts_platform_capture_milestone_011(const RtsAppConfig *cfg) {
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
    double R = app->world.planet.radius_m;

    /* Shot 1: Continuous Spherical Multi-LOD Terrain, Shoreline & Animated Solaris Ocean Bay with Fleet Carrier + Attack Sub */
    app->world.commander.at_holo_globe = false;
    app->world.commander.frame_id = app->world.planet_frame_id;
    app->world.commander.occupied_entity_id = 0u;
    rts_renderer_set_active_interior(&app->renderer, NULL, NULL);

    uint32_t carrier_id = rts_world_spawn_from_blueprint(&app->world, "ship_fleet_carrier", "Fleet Carrier Vanguard",
                                                         1u, app->world.planet_frame_id, rts_v3d(175.0, R + 1.0, 22.0), true);
    uint32_t sub_id = rts_world_spawn_from_blueprint(&app->world, "ship_attack_sub", "Submarine Abyss-3",
                                                     1u, app->world.planet_frame_id, rts_v3d(145.0, R - 0.8, 52.0), true);
    rts_vehicle_physics_bind(&app->vphys, &app->world, sub_id);
    app->vphys.ballast_depth_m = -12.5;
    app->vphys.wake_intensity = 0.78f;
    app->vphys.wave_roll_rad = 0.035;
    (void)carrier_id;

    app->world.commander.local_pos = rts_v3d(78.0, R + 22.0, 68.0);
    RtsVec3d bay_look = rts_v3d_sub(rts_v3d(165.0, R, 26.0), app->world.commander.local_pos);
    app->world.commander.yaw_rad = atan2(bay_look.x, -bay_look.z);
    app->world.commander.pitch_rad = -0.18;
    rts_renderer_set_live_status(&app->renderer, "SPHERICAL MULTI-LOD TERRAIN & OCEAN", "",
                                 "CONTINUOUS CUBE-SPHERE TERRAIN + ANIMATED SOLARIS OCEAN BAY | CARRIER & SUBMARINE BALLAST PHYSICS");
    if (!save_live_interior_shot(app, cfg->milestone_011_dir, "m011_01_spherical_terrain_ocean.png", 2.1f)) rc = 1;

    /* Shot 2: Heavy Vehicle Lifter Dropship VTOL Ascent With Clamped Plasma Tank Cargo */
    uint32_t lifter_id = rts_world_spawn_from_blueprint(&app->world, "air_heavy_lifter", "Heavy Lifter Atlas-1",
                                                        1u, app->world.planet_frame_id, rts_v3d(20.0, R + 24.0, 38.0), true);
    uint32_t cargo_tank = rts_world_spawn_from_blueprint(&app->world, "veh_plasma_tank", "Clamped Vanguard Tank",
                                                         1u, app->world.planet_frame_id, rts_v3d(20.0, R + 22.0, 38.0), false);
    rts_vehicle_physics_bind(&app->vphys, &app->world, lifter_id);
    rts_vehicle_clamp_cargo(&app->vphys, &app->world, lifter_id, cargo_tank);
    app->vphys.vtol_blend = 0.72;
    app->vphys.airspeed_mps = 34.0;
    app->vphys.altitude_agl_m = 24.0;
    RtsEntity *lf = rts_world_get_entity(&app->world, lifter_id);
    RtsEntity *ct = rts_world_get_entity(&app->world, cargo_tank);
    if (lf && ct) {
        lf->pos = rts_v3d(20.0, R + 24.0, 38.0);
        ct->pos = rts_v3d(20.0, R + 18.2, 38.0);
    }
    app->world.commander.local_pos = rts_v3d(-24.0, R + 16.0, 72.0);
    RtsVec3d lf_look = rts_v3d_sub(rts_v3d(20.0, R + 21.0, 38.0), app->world.commander.local_pos);
    app->world.commander.yaw_rad = atan2(lf_look.x, -lf_look.z);
    app->world.commander.pitch_rad = 0.08;
    rts_renderer_set_live_status(&app->renderer, "CLASS 3 VTOL & CARGO CLAMP PHYSICS", "",
                                 "HEAVY LIFTER DROPSHIP [K] CLAMPING PLASMA TANK UNDER BAY FOR ORBITAL INSERTION!");
    if (!save_live_interior_shot(app, cfg->milestone_011_dir, "m011_02_dropship_cargo_vtol.png", 3.4f)) rc = 1;

    /* Shot 3: Unified Live 64-Bit Combat — EMP-Prime x2.4 Halo, Beam Lance, Orbital Ion Strike, Shield Ripples & Voxel Crater */
    app->vphys.entity_id = 0u;
    app->vphys.time_compression = 1;
    uint32_t ion_id = rts_world_spawn_from_blueprint(&app->world, "bldg_orbital_cannon", "Solaris Ion Cannon",
                                                     1u, app->world.planet_frame_id, rts_v3d(-34.0, R + 0.5, 42.0), false);
    uint32_t enemy_a = rts_world_spawn_from_blueprint(&app->world, "veh_siege_hovertank", "Crimson Siege 1",
                                                      2u, app->world.planet_frame_id, rts_v3d(34.0, R + 2.0, 44.0), false);
    uint32_t enemy_b = rts_world_spawn_from_blueprint(&app->world, "veh_plasma_tank", "Crimson Raider 2",
                                                      2u, app->world.planet_frame_id, rts_v3d(18.0, R + 2.0, 26.0), false);
    uint32_t ally_t = rts_world_spawn_from_blueprint(&app->world, "veh_plasma_tank", "Vanguard Spear 1",
                                                     1u, app->world.planet_frame_id, rts_v3d(-16.0, R + 2.0, 34.0), false);
    rts_combat_apply_emp_prime(&app->combat, &app->world, enemy_a, 6.0f);
    rts_combat_fire(&app->combat, &app->world, ion_id, enemy_a, rts_v3d(0, 0, 0), RTS_WPN_ORBITAL_ION);
    rts_combat_fire(&app->combat, &app->world, ally_t, enemy_b, rts_v3d(0, 0, 0), RTS_WPN_BEAM_LANCE);
    rts_combat_fire(&app->combat, &app->world, enemy_b, ally_t, rts_v3d(0, 0, 0), RTS_WPN_HOMING_TORPEDO);
    rts_combat_fire(&app->combat, &app->world, ally_t, enemy_a, rts_v3d(0, 0, 0), RTS_WPN_EMP_PULSE);
    rts_combat_step(&app->combat, &app->world, NULL, 0.04);
    rts_combat_carve_voxel_crater(&app->combat, rts_v3d(4.0, R + 1.0, 56.0), 5.0f);

    app->world.commander.local_pos = rts_v3d(4.0, R + 18.0, 96.0);
    RtsVec3d cb_look = rts_v3d_sub(rts_v3d(4.0, R + 4.0, 38.0), app->world.commander.local_pos);
    app->world.commander.yaw_rad = atan2(cb_look.x, -cb_look.z);
    app->world.commander.pitch_rad = -0.22;
    rts_renderer_set_live_status(&app->renderer, "UNIFIED 64-BIT LIVE COMBAT & VOXEL CRATERS", "",
                                 "EMP-PRIME X2.4 SYNERGY, ORBITAL ION CANNON, BEAM LANCE, HEX-SHIELD RIPPLES & VOXEL CRATER!");
    if (!save_live_interior_shot(app, cfg->milestone_011_dir, "m011_03_unified_live_combat_emp.png", 4.7f)) rc = 1;

    /* Shot 4: Spacecraft 6-DOF Orbital Re-entry Over Curved Helios Prime Horizon With Plasma Heat Shield Cone */
    uint32_t reentry_id = rts_world_spawn_from_blueprint(&app->world, "air_dropship", "Valkyrie Re-Entry",
                                                         1u, app->world.planet_frame_id, rts_v3d(-8.0, R + 115000.0, -32.0), false);
    RtsEntity *dr = rts_world_get_entity(&app->world, reentry_id);
    if (dr) {
        dr->vel = rts_v3d(0.0, -420.0, -5400.0);
        dr->rot = rts_quat_mul(rts_quat_from_axis_angle(rts_v3d(0, 1, 0), 0.55),
                               rts_quat_from_axis_angle(rts_v3d(1, 0, 0), -0.28));
        dr->orbital_mode = true;
        rts_vehicle_physics_bind(&app->vphys, &app->world, dr->id);
        app->vphys.prograde_dv_mps = 5416.0;
        app->vphys.reentry_heat_wm2 = 186000.0;
        app->vphys.reentry_glow = 0.88f;
        app->vphys.time_compression = 4;
    }
    app->world.commander.local_pos = rts_v3d(14.0, R + 115009.0, 2.0);
    RtsVec3d re_look = rts_v3d_sub(rts_v3d(-12.0, R + 114998.0, -36.0), app->world.commander.local_pos);
    app->world.commander.yaw_rad = atan2(re_look.x, -re_look.z);
    app->world.commander.pitch_rad = -0.18;
    rts_renderer_set_live_status(&app->renderer, "6-DOF SPACECRAFT ORBITAL RE-ENTRY", "",
                                 "ORBITAL ATMOSPHERIC RE-ENTRY: PLASMA HEAT ENVELOPE, CLOUD LAYER & 4X TIME COMPRESSION");
    if (!save_live_interior_shot(app, cfg->milestone_011_dir, "m011_04_orbital_reentry_plasma.png", 6.2f)) rc = 1;

    RtsGfxStats st = app->renderer.gfx.get_stats(app->renderer.gfx.state);
    printf("[milestone_011] OpenGL draw calls: %u, screenshots: %u, gl_errors: %u\n",
           st.draw_calls, st.screenshots_saved, st.gl_errors);
    if (st.gl_errors != 0u) {
        fprintf(stderr, "[milestone_011] FAIL: OpenGL reported %u errors\n", st.gl_errors);
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
