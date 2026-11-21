/* SPDX-License-Identifier: MIT */
#include "rts/controller.h"
#include "rts/crew.h"
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/interior_gen.h"
#include "rts/lua_vm.h"
#include "rts/vfs.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include "unity.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void test_mega_interiors_all_76_blueprints_and_bfs_connectivity(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));

    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsInputRouter *in = (RtsInputRouter *)calloc(1, sizeof(RtsInputRouter));
    RtsVoxelChunk *vx = (RtsVoxelChunk *)calloc(1, sizeof(RtsVoxelChunk));
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    RtsLuaVm *vm = (RtsLuaVm *)calloc(1, sizeof(RtsLuaVm));
    RtsMegaInterior *mega = (RtsMegaInterior *)calloc(1, sizeof(RtsMegaInterior));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(mega);

    rts_world_init(w, rts_planet_earthlike(42u), 42u);
    rts_input_init(in, 1280.0f, 720.0f, false);
    rts_voxel_init(vx, 1.0f);
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1280, 720));
    TEST_ASSERT_TRUE(rts_lua_init(vm, &vfs, w, in, vx, rr));
    TEST_ASSERT_TRUE(rts_lua_exec_vfs_script(vm, "main.lua"));
    TEST_ASSERT_EQUAL_UINT32(76u, (uint32_t)w->blueprint_count);

    /* Validate every room kind human-readable name including default fallback */
    for (int k = 0; k <= (int)RTS_ROOM_TRAM_STATION + 1; ++k) {
        const char *nm = rts_room_kind_name((RtsRoomKind)k);
        TEST_ASSERT_NOT_NULL(nm);
        TEST_ASSERT_TRUE(nm[0] != '\0');
    }

    uint32_t max_rooms_seen = 0u;
    uint32_t min_rooms_seen = 9999u;
    for (size_t i = 0; i < w->blueprint_count; ++i) {
        const RtsConstructBlueprint *bp = &w->blueprints[i];
        TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, bp, 1u));
        uint32_t reachable = 0u;
        TEST_ASSERT_TRUE_MESSAGE(rts_mega_verify_connectivity(mega, &reachable), bp->id);
        TEST_ASSERT_EQUAL_UINT32(mega->total_rooms, reachable);
        if (mega->total_rooms > max_rooms_seen) max_rooms_seen = mega->total_rooms;
        if (mega->total_rooms < min_rooms_seen) min_rooms_seen = mega->total_rooms;

        if (strcmp(bp->id, "space_command_dreadnought") == 0 || strcmp(bp->id, "stn_orbital_shipyard") == 0) {
            TEST_ASSERT_EQUAL_UINT32(420u, mega->total_rooms);
            TEST_ASSERT_EQUAL_UINT16(20u, mega->deck_count);
            TEST_ASSERT_EQUAL_UINT16(7u, mega->cols);
            TEST_ASSERT_EQUAL_UINT16(3u, mega->rows);
        } else if (strcmp(bp->id, "air_starfighter") == 0) {
            TEST_ASSERT_EQUAL_UINT32(1u, mega->total_rooms);
            TEST_ASSERT_EQUAL_UINT16(1u, mega->deck_count);
        } else if (strcmp(bp->id, "ship_fleet_carrier") == 0) {
            TEST_ASSERT_EQUAL_UINT32(150u, mega->total_rooms);
            TEST_ASSERT_EQUAL_UINT16(10u, mega->deck_count);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, min_rooms_seen);
    TEST_ASSERT_EQUAL_UINT32(420u, max_rooms_seen);

    /* Guard checks on invalid inputs & edge cases */
    TEST_ASSERT_FALSE(rts_mega_generate_for_blueprint(NULL, &w->blueprints[0], 1u));
    TEST_ASSERT_FALSE(rts_mega_generate_for_blueprint(mega, NULL, 1u));
    TEST_ASSERT_FALSE(rts_mega_verify_connectivity(NULL, NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, rts_mega_find_deck_with_kind(NULL, RTS_ROOM_REACTOR));
    TEST_ASSERT_FALSE(rts_mega_hydrate_deck(NULL, w, 1u, 0u, NULL));
    TEST_ASSERT_FALSE(rts_mega_hydrate_deck(mega, w, 99999u, 0u, NULL));
    TEST_ASSERT_FALSE(rts_mega_switch_deck(NULL, w, 0u));
    TEST_ASSERT_FALSE(rts_mega_switch_deck(mega, w, 999u));

    /* Break a doorway in a 2-room layout to verify BFS detects disconnected room */
    const RtsConstructBlueprint *drop_bp = rts_world_find_blueprint(w, "air_dropship");
    TEST_ASSERT_NOT_NULL(drop_bp);
    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, drop_bp, 1u));
    mega->rooms[0].door_mask = 0u;
    mega->rooms[1].door_mask = 0u;
    mega->rooms[2].door_mask = 0u;
    TEST_ASSERT_FALSE(rts_mega_verify_connectivity(mega, NULL));

    rts_lua_shutdown(vm);
    rts_renderer_shutdown(rr);
    free(mega);
    free(vm);
    free(rr);
    free(vx);
    free(in);
    free(w);
}

static void test_multi_room_doorway_walking_and_turbolift_deck_streaming(void) {
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsMegaInterior *mega = (RtsMegaInterior *)calloc(1, sizeof(RtsMegaInterior));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(mega);

    rts_world_init(w, rts_planet_earthlike(42u), 42u);

    RtsConstructBlueprint dread_bp;
    memset(&dread_bp, 0, sizeof(dread_bp));
    snprintf(dread_bp.id, sizeof(dread_bp.id), "space_command_dreadnought");
    snprintf(dread_bp.name, sizeof(dread_bp.name), "Fleet Command Dreadnought");
    dread_bp.construct_class = RTS_CLASS_SPACECRAFT;
    dread_bp.dimensions_m = (RtsVec3f){680.0f, 180.0f, 125.0f};
    dread_bp.max_hull = 22000.0f;
    dread_bp.max_shield = 16000.0f;
    dread_bp.max_speed_mps = 120.0f;
    dread_bp.crew_capacity = 240u;
    dread_bp.room_count = 24u;
    dread_bp.has_holo_globe = true;
    dread_bp.has_escape_pod = true;
    dread_bp.can_orbit = true;
    TEST_ASSERT_TRUE(rts_world_register_blueprint(w, &dread_bp));

    RtsConstructBlueprint decoy_bp = dread_bp;
    snprintf(decoy_bp.id, sizeof(decoy_bp.id), "bldg_decoy");
    decoy_bp.room_count = 2u;
    TEST_ASSERT_TRUE(rts_world_register_blueprint(w, &decoy_bp));

    uint32_t decoy_id = rts_world_spawn_from_blueprint(w, "bldg_decoy", "Decoy", 1u, w->planet_frame_id,
                                                       rts_v3d(0.0, w->planet.radius_m, 0.0), true);
    RtsEntity *decoy = rts_world_get_entity(w, decoy_id);
    uint32_t decoy_room = rts_interior_add_room(&w->interiors, decoy->own_frame_id, "Decoy Room",
                                                (RtsAabb){rts_v3d(-4, 0, -4), rts_v3d(4, 3, 4)}, (RtsVec3f){1, 1, 1}, true);
    TEST_ASSERT_NOT_NULL(rts_interior_get_room(&w->interiors, decoy_room));
    TEST_ASSERT_NULL(rts_interior_get_room(&w->interiors, 99999u));

    uint32_t dread_id = rts_world_spawn_from_blueprint(w, "space_command_dreadnought", "Dreadnought Solaris", 1u,
                                                       w->planet_frame_id,
                                                       rts_v3d(0.0, w->planet.radius_m + 400000.0, 0.0), true);
    RtsEntity *dread = rts_world_get_entity(w, dread_id);
    TEST_ASSERT_NOT_NULL(dread);

    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, &dread_bp, dread_id));
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 0u));

    /* Verify decoy frame room was preserved when dreadnought deck was hydrated */
    TEST_ASSERT_NOT_NULL(rts_interior_get_room(&w->interiors, decoy_room));

    /* Commander starts inside Deck 1 War Room (gz=0, z in [-15, -5]). Walk South (+Z) through open doorway into Turbolift Hub (gz=1, z in [-5, +5])! */
    const RtsInteriorRoom *start_rm = rts_interior_find_room_at(&w->interiors, dread->own_frame_id, w->commander.local_pos);
    TEST_ASSERT_NOT_NULL(start_rm);
    TEST_ASSERT_EQUAL_INT(RTS_ROOM_WAR_ROOM, start_rm->kind);

    RtsCommanderController ctrl;
    rts_controller_init(&ctrl);
    w->commander.yaw_rad = 0.0; /* forward is -Z, so move_y = -1 walks +Z South through the doorway */
    RtsInputIntent intent;
    memset(&intent, 0, sizeof(intent));
    intent.move_y = -1.0f;
    for (int t = 0; t < 30; ++t) {
        rts_controller_step(&ctrl, w, &intent, 1.0 / 30.0);
    }
    const RtsInteriorRoom *mid_rm = rts_interior_find_room_at(&w->interiors, dread->own_frame_id, w->commander.local_pos);
    TEST_ASSERT_NOT_NULL(mid_rm);
    TEST_ASSERT_NOT_EQUAL(start_rm->id, mid_rm->id);
    TEST_ASSERT_EQUAL_INT(RTS_ROOM_TURBOLIFT, mid_rm->kind);

    /* Continue walking South through a SECOND open doorway directly into the Boarding Airlock! */
    for (int t = 0; t < 40; ++t) {
        rts_controller_step(&ctrl, w, &intent, 1.0 / 30.0);
    }
    const RtsInteriorRoom *after_rm = rts_interior_find_room_at(&w->interiors, dread->own_frame_id, w->commander.local_pos);
    TEST_ASSERT_NOT_NULL(after_rm);
    TEST_ASSERT_EQUAL_INT(RTS_ROOM_AIRLOCK, after_rm->kind);

    /* Walk all the way against the North exterior perimeter wall to verify axis-sliding clamps cleanly inside bounds */
    w->commander.local_pos = rts_v3d(0.0, 1.75, -14.2);
    intent.move_y = 1.0f;
    intent.move_x = 0.6f;
    for (int t = 0; t < 40; ++t) {
        rts_controller_step(&ctrl, w, &intent, 1.0 / 30.0);
    }
    TEST_ASSERT_TRUE(w->commander.local_pos.z >= -15.0);
    TEST_ASSERT_TRUE(w->commander.local_pos.x > 0.5); /* Slid cleanly along X while clamped on Z! */

    /* Switch vertically to Hangar Deck (Deck 1), Mid Deck (Deck 5), and Reactor Deck (Deck 13) */
    uint16_t hangar_deck = rts_mega_find_deck_with_kind(mega, RTS_ROOM_HANGAR);
    uint16_t reactor_deck = rts_mega_find_deck_with_kind(mega, RTS_ROOM_REACTOR);
    TEST_ASSERT_EQUAL_UINT16(1u, hangar_deck);
    TEST_ASSERT_TRUE(reactor_deck >= 12u);

    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, hangar_deck));
    TEST_ASSERT_EQUAL_UINT16(hangar_deck, mega->active_deck);
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 5u));
    TEST_ASSERT_EQUAL_UINT16(5u, mega->active_deck);
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, reactor_deck));
    TEST_ASSERT_EQUAL_UINT16(reactor_deck, mega->active_deck);
    /* Nonexistent room kind lookup returns 0 */
    TEST_ASSERT_EQUAL_UINT16(0u, rts_mega_find_deck_with_kind(mega, RTS_ROOM_GENERIC));

    free(mega);
    free(w);
}

static void test_living_crew_12_roles_patrol_repair_and_counter_boarding(void) {
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsMegaInterior *mega = (RtsMegaInterior *)calloc(1, sizeof(RtsMegaInterior));
    RtsCrewDeckSim *crew = (RtsCrewDeckSim *)calloc(1, sizeof(RtsCrewDeckSim));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(mega);
    TEST_ASSERT_NOT_NULL(crew);

    rts_world_init(w, rts_planet_earthlike(42u), 42u);
    rts_crew_init(NULL); /* safe null check */

    /* Exercise all 12 role names, tints, and hostile tint */
    for (int r = 0; r <= (int)RTS_ROLE_COUNT; ++r) {
        const char *rn = rts_crew_role_name((RtsCrewRoleId)r);
        TEST_ASSERT_NOT_NULL(rn);
        RtsVec4f t1 = rts_crew_role_tint((RtsCrewRoleId)r, 1u);
        RtsVec4f t2 = rts_crew_role_tint((RtsCrewRoleId)r, 2u);
        TEST_ASSERT_TRUE(t1.w > 0.5f && t2.x > 0.9f);
    }

    RtsConstructBlueprint dread_bp;
    memset(&dread_bp, 0, sizeof(dread_bp));
    snprintf(dread_bp.id, sizeof(dread_bp.id), "space_command_dreadnought");
    snprintf(dread_bp.name, sizeof(dread_bp.name), "Fleet Command Dreadnought");
    dread_bp.construct_class = RTS_CLASS_SPACECRAFT;
    dread_bp.dimensions_m = (RtsVec3f){680.0f, 180.0f, 125.0f};
    dread_bp.max_hull = 22000.0f;
    dread_bp.max_shield = 16000.0f;
    dread_bp.max_speed_mps = 120.0f;
    dread_bp.crew_capacity = 240u;
    dread_bp.room_count = 24u;
    dread_bp.has_holo_globe = true;
    dread_bp.has_escape_pod = true;
    dread_bp.can_orbit = true;
    rts_world_register_blueprint(w, &dread_bp);

    uint32_t dread_id = rts_world_spawn_from_blueprint(w, "space_command_dreadnought", "Dreadnought Solaris", 1u,
                                                       w->planet_frame_id,
                                                       rts_v3d(0.0, w->planet.radius_m + 400000.0, 0.0), true);
    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, &dread_bp, dread_id));

    /* Collect distinct roles populated across Command Deck (0), Hangar Deck (1), Mid Deck (5), and Reactor Deck (13) */
    uint32_t combined_roles = 0u;
    uint16_t sample_decks[4] = {0u, 1u, 5u, 13u};
    for (int d = 0; d < 4; ++d) {
        TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, sample_decks[d]));
        size_t n = rts_crew_populate_deck(crew, mega, w);
        TEST_ASSERT_TRUE(n >= 40u);
        combined_roles |= crew->distinct_role_mask;
    }
    /* All 12 roles (bits 0..11 = 0x0FFF) must be staffed across the ship's decks! */
    TEST_ASSERT_EQUAL_HEX32(0x0FFFu, combined_roles);

    /* On Deck 0 (has War Room, Bridge, Airlock, Armory, Corridor, Medbay -> no Reactor, tests fallback room damage + breachers) */
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 0u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 40u);
    rts_crew_set_red_alert(crew, mega, w, true);
    TEST_ASSERT_TRUE(crew->red_alert);

    for (int t = 0; t < 900; ++t) {
        rts_crew_step(crew, mega, w, 1.0 / 30.0);
    }
    TEST_ASSERT_TRUE(crew->repairs_completed >= 1u);
    TEST_ASSERT_TRUE(crew->breachers_neutralized >= 1u);

    /* Stand down Red Alert -> clears any remaining hostiles cleanly */
    rts_crew_set_red_alert(crew, mega, w, false);
    TEST_ASSERT_FALSE(crew->red_alert);

    /* Also verify Reactor Deck (Deck 13) damages Reactor Core directly and Engineers repair it */
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 13u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 40u);
    rts_crew_set_red_alert(crew, mega, w, true);
    for (int t = 0; t < 900; ++t) {
        rts_crew_step(crew, mega, w, 1.0 / 30.0);
    }
    TEST_ASSERT_TRUE(crew->repairs_completed >= 1u);

    /* Guard checks on null/tiny crew capacity */
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)rts_crew_populate_deck(NULL, mega, w));
    rts_crew_set_red_alert(NULL, mega, w, true);
    rts_crew_step(NULL, mega, w, 0.1);
    rts_crew_step(crew, mega, w, 0.0);

    RtsConstructBlueprint tiny_bp = dread_bp;
    snprintf(tiny_bp.id, sizeof(tiny_bp.id), "air_starfighter");
    tiny_bp.room_count = 1u;
    tiny_bp.crew_capacity = 1u;
    tiny_bp.has_holo_globe = false;
    rts_world_register_blueprint(w, &tiny_bp);
    uint32_t sf_id = rts_world_spawn_from_blueprint(w, "air_starfighter", "SF-1", 1u, w->planet_frame_id,
                                                    rts_v3d(0, w->planet.radius_m + 50, 0), true);
    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, &tiny_bp, sf_id));
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 0u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 1u);

    free(crew);
    free(mega);
    free(w);
}

static void test_renderer_live_mega_interiors_all_room_kinds_and_minimap(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));

    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsInputRouter *in = (RtsInputRouter *)calloc(1, sizeof(RtsInputRouter));
    RtsVoxelChunk *vx = (RtsVoxelChunk *)calloc(1, sizeof(RtsVoxelChunk));
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    RtsLuaVm *vm = (RtsLuaVm *)calloc(1, sizeof(RtsLuaVm));
    RtsMegaInterior *mega = (RtsMegaInterior *)calloc(1, sizeof(RtsMegaInterior));
    RtsCrewDeckSim *crew = (RtsCrewDeckSim *)calloc(1, sizeof(RtsCrewDeckSim));
    TEST_ASSERT_NOT_NULL(w);

    rts_world_init(w, rts_planet_earthlike(42u), 42u);
    rts_input_init(in, 1280.0f, 720.0f, false);
    rts_voxel_init(vx, 1.0f);
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1280, 720));
    TEST_ASSERT_TRUE(rts_lua_init(vm, &vfs, w, in, vx, rr));
    TEST_ASSERT_TRUE(rts_lua_exec_vfs_script(vm, "main.lua"));

    uint32_t dread_id = 0u;
    for (size_t i = 0; i < w->entity_count; ++i) {
        if (strcmp(w->entities[i].blueprint_id, "space_command_dreadnought") == 0) {
            dread_id = w->entities[i].id;
            break;
        }
    }
    TEST_ASSERT_TRUE(dread_id != 0u);
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, "space_command_dreadnought");
    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(mega, bp, dread_id));

    /* Render Deck 0 (War Room + Bridge + Turbolift + Corridor + Armory + Medbay + Escape Pod + Airlock) */
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 0u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 40u);
    rts_renderer_set_active_interior(rr, mega, crew);
    rts_renderer_render_live_world(rr, w, in, 1.0f);

    /* Render Deck 1 (Hangar Starfighters + Mag-Tram + Cargo) */
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 1u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 40u);
    rts_renderer_render_live_world(rr, w, in, 2.0f);

    /* Render Deck 13 (Reactor Tokamak Core + Shield Core + Engineering) under Red Alert with live Repair & Combat beams */
    TEST_ASSERT_TRUE(rts_mega_switch_deck(mega, w, 13u));
    TEST_ASSERT_TRUE(rts_crew_populate_deck(crew, mega, w) >= 40u);
    rts_crew_set_red_alert(crew, mega, w, true);
    for (int t = 0; t < 20; ++t) {
        rts_crew_step(crew, mega, w, 1.0 / 30.0);
    }
    rts_renderer_render_live_world(rr, w, in, 3.0f);

    /* Also render with NULL active_mega/crew to cover fallback minimap & crew branch */
    rts_renderer_set_active_interior(rr, NULL, NULL);
    rts_renderer_render_live_world(rr, w, in, 4.0f);

    RtsGfxStats st = rr->gfx.get_stats(rr->gfx.state);
    TEST_ASSERT_TRUE(st.draw_calls > 150u);
    TEST_ASSERT_EQUAL_UINT32(0u, st.gl_errors);

    rts_lua_shutdown(vm);
    rts_renderer_shutdown(rr);
    free(crew);
    free(mega);
    free(vm);
    free(rr);
    free(vx);
    free(in);
    free(w);
}

void register_mega_interiors_and_crew_tests(void) {
    RUN_TEST(test_mega_interiors_all_76_blueprints_and_bfs_connectivity);
    RUN_TEST(test_multi_room_doorway_walking_and_turbolift_deck_streaming);
    RUN_TEST(test_living_crew_12_roles_patrol_repair_and_counter_boarding);
    RUN_TEST(test_renderer_live_mega_interiors_all_room_kinds_and_minimap);
}
