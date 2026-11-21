/* SPDX-License-Identifier: MIT */
#include "rts/boarding.h"
#include "rts/controller.h"
#include "rts/crew.h"
#include "rts/economy.h"
#include "rts/field_gear.h"
#include "rts/gfx.h"
#include "rts/hologlobe_ui.h"
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

typedef struct ToolkitTestRig {
    RtsVfs vfs;
    RtsWorld world;
    RtsInputRouter input;
    RtsVoxelChunk voxel;
    RtsEconomySystem eco;
    RtsCommanderController ctrl;
    RtsBoardingSystem boarding;
    RtsRetroRenderer rr;
    RtsLuaVm vm;
    RtsMegaInterior mega;
    RtsCrewDeckSim crew;
    RtsHoloGlobeState holo;
    RtsFieldGearState gear;
} ToolkitTestRig;

static ToolkitTestRig *create_rig(void) {
    ToolkitTestRig *r = (ToolkitTestRig *)calloc(1, sizeof(ToolkitTestRig));
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_TRUE(rts_vfs_init(&r->vfs, RTS_TEST_DATA_DIR, NULL));
    rts_world_init(&r->world, rts_planet_earthlike(42u), 42u);
    rts_input_init(&r->input, 1280.0f, 720.0f, false);
    rts_voxel_init(&r->voxel, 1.0f);
    rts_economy_init(&r->eco);
    rts_controller_init(&r->ctrl);
    rts_boarding_init(&r->boarding);
    rts_crew_init(&r->crew);
    rts_hologlobe_init(&r->holo);
    rts_field_gear_init(&r->gear);
    TEST_ASSERT_TRUE(rts_renderer_init(&r->rr, rts_gfx_null_create(), &r->vfs, 1280, 720));
    TEST_ASSERT_TRUE(rts_lua_init(&r->vm, &r->vfs, &r->world, &r->input, &r->voxel, &r->rr));
    rts_lua_attach_phase2(&r->vm, NULL, &r->eco, &r->ctrl);
    rts_lua_attach_phase3(&r->vm, &r->boarding);
    TEST_ASSERT_TRUE(rts_lua_exec_vfs_script(&r->vm, "main.lua"));
    return r;
}

static void destroy_rig(ToolkitTestRig *r) {
    rts_lua_shutdown(&r->vm);
    rts_renderer_shutdown(&r->rr);
    free(r);
}

static uint32_t find_bp(const RtsWorld *w, const char *bp_id, uint8_t fac) {
    for (size_t i = 0; i < w->entity_count; ++i) {
        if (w->entities[i].active && w->entities[i].faction_id == fac &&
            strcmp(w->entities[i].blueprint_id, bp_id) == 0) {
            return w->entities[i].id;
        }
    }
    return 0u;
}

static void test_hologlobe_raycast_fow_orders_and_3d_placement(void) {
    ToolkitTestRig *r = create_rig();

    /* Exercise mode names, orbit drag, zoom, and altitude interpolation */
    for (int m = 0; m <= (int)RTS_HOLO_MODE_COUNT; ++m) {
        TEST_ASSERT_NOT_NULL(rts_hologlobe_mode_name((RtsHoloOrderMode)m));
    }
    rts_hologlobe_init(NULL);
    rts_hologlobe_orbit_drag(&r->holo, 0.2, -0.1);
    rts_hologlobe_zoom(&r->holo, -1.0);
    double high_alt = rts_hologlobe_view_altitude_km(&r->holo);
    rts_hologlobe_zoom(&r->holo, 2.5);
    double low_alt = rts_hologlobe_view_altitude_km(&r->holo);
    TEST_ASSERT_TRUE(high_alt > 10000.0 && low_alt < 200.0);
    rts_hologlobe_init(&r->holo);

    /* Exact invertible round-trip between 64-bit world surface and 3D Holo-Globe */
    RtsVec3d gcenter = rts_v3d(0.0, 1.65, -4.0);
    double gradius = 0.92;
    RtsVec3d orig_w = rts_v3d(120.0, sqrt(r->world.planet.radius_m * r->world.planet.radius_m - 120.0 * 120.0 - 80.0 * 80.0), -80.0);
    RtsVec3d gp = rts_hologlobe_world_to_globe(&r->holo, &r->world.planet, orig_w, gcenter, gradius);
    RtsVec3d back_w = rts_hologlobe_globe_to_world(&r->holo, &r->world.planet, gp, gcenter, gradius, false);
    TEST_ASSERT_TRUE(rts_v3d_dist(orig_w, back_w) < 5.0);

    /* Raycast pick friendly Plasma Tank blip on 3D Holo-Globe */
    uint32_t tank_id = find_bp(&r->world, "veh_plasma_tank", 1u);
    uint32_t dropship_id = find_bp(&r->world, "air_dropship", 1u);
    uint32_t enemy_tank_id = find_bp(&r->world, "veh_plasma_tank", 2u);
    uint32_t shipyard_id = find_bp(&r->world, "stn_orbital_shipyard", 1u);
    uint32_t enemy_dread_id = find_bp(&r->world, "space_siege_dreadnought", 2u);
    const RtsEntity *tank = rts_world_get_entity_const(&r->world, tank_id);
    TEST_ASSERT_NOT_NULL(tank);

    RtsVec3d tank_gp = rts_hologlobe_world_to_globe(&r->holo, &r->world.planet, tank->pos, gcenter, gradius);
    RtsRay pick_ray = {rts_v3d(tank_gp.x, tank_gp.y, tank_gp.z + 2.5), rts_v3d(0.0, 0.0, -1.0)};
    TEST_ASSERT_TRUE(rts_hologlobe_raycast(&r->holo, &r->world, 1u, pick_ray, gcenter, gradius));
    TEST_ASSERT_EQUAL_UINT32(tank_id, r->holo.hovered_entity_id);
    TEST_ASSERT_TRUE(rts_hologlobe_select_entity(&r->holo, tank_id, false));
    TEST_ASSERT_TRUE(rts_hologlobe_select_entity(&r->holo, tank_id, true)); /* idempotent additive */
    TEST_ASSERT_TRUE(rts_hologlobe_is_selected(&r->holo, tank_id));

    /* Verify Fog-of-War hides an enemy on the far side of the spherical planet (0, -R, 0) from 3D Holo-Globe picking */
    uint32_t far_enemy_id = rts_world_spawn_from_blueprint(&r->world, "veh_plasma_tank", "Hidden Far Raider", 2u,
                                                           r->world.planet_frame_id,
                                                           rts_v3d(0.0, -r->world.planet.radius_m, 0.0), false);
    const RtsEntity *far_e = rts_world_get_entity_const(&r->world, far_enemy_id);
    RtsVec3d far_gp = rts_hologlobe_world_to_globe(&r->holo, &r->world.planet, far_e->pos, gcenter, gradius);
    RtsRay far_ray = {rts_v3d(far_gp.x, far_gp.y, far_gp.z - 2.0), rts_v3d(0.0, 0.0, 1.0)};
    rts_hologlobe_raycast(&r->holo, &r->world, 1u, far_ray, gcenter, gradius);
    TEST_ASSERT_NOT_EQUAL(far_enemy_id, r->holo.hovered_entity_id);

    /* Box-select all friendly constructs on the globe */
    size_t boxed = rts_hologlobe_box_select(&r->holo, &r->world, 1u, gcenter, gradius,
                                            rts_v3d(-10.0, -10.0, -10.0), rts_v3d(10.0, 10.0, 10.0));
    TEST_ASSERT_TRUE(boxed >= 4u);

    /* Issue all 7 arbitrary order modes cleanly */
    rts_hologlobe_clear_selection(&r->holo);
    TEST_ASSERT_FALSE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_MOVE, orig_w, 0u));
    rts_hologlobe_select_entity(&r->holo, tank_id, false);
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_MOVE, orig_w, 0u));
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_ATTACK, orig_w, enemy_tank_id));

    rts_hologlobe_select_entity(&r->holo, dropship_id, false);
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_ORBIT, orig_w, 0u));
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_HOHMANN, orig_w, 0u));
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_DOCK, orig_w, shipyard_id));
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_REENTRY, orig_w, 0u));
    TEST_ASSERT_TRUE(rts_hologlobe_issue_order(&r->holo, &r->world, &r->ctrl, &r->boarding, 1u, RTS_HOLO_MODE_BOARD, orig_w, enemy_dread_id));

    /* Cycle buildings & validate/confirm 3D placement (surface building + orbital station + invalid altitude + obstructed) */
    const RtsConstructBlueprint *cb = rts_hologlobe_cycle_building(&r->holo, &r->world, 1);
    TEST_ASSERT_NOT_NULL(cb);
    TEST_ASSERT_NOT_NULL(rts_hologlobe_cycle_building(&r->holo, &r->world, -1));
    /* Ray hit on surface globe in PLACE_BUILDING mode */
    RtsRay surface_ray = {rts_v3d(gcenter.x, gcenter.y, gcenter.z + 3.0), rts_v3d(0.0, 0.0, -1.0)};
    TEST_ASSERT_TRUE(rts_hologlobe_raycast(&r->holo, &r->world, 1u, surface_ray, gcenter, gradius));

    /* Valid surface building placement */
    RtsVec3d clear_site = rts_v3d(-140.0, r->world.planet.radius_m, 95.0);
    uint32_t built_bldg = rts_hologlobe_confirm_placement(&r->holo, &r->world, &r->eco, 1u, "bldg_solar_geothermal", clear_site);
    TEST_ASSERT_TRUE(built_bldg > 0u);
    /* Re-placing on exact same coordinates must fail clearance check */
    TEST_ASSERT_EQUAL_UINT32(0u, rts_hologlobe_confirm_placement(&r->holo, &r->world, &r->eco, 1u, "bldg_solar_geothermal", clear_site));
    /* Placing a building in orbit must fail altitude check */
    TEST_ASSERT_FALSE(rts_hologlobe_validate_placement(&r->holo, &r->world, &r->eco, 1u, "bldg_solar_geothermal",
                                                       rts_v3d(0.0, r->world.planet.radius_m + 400000.0, 0.0)));
    /* Placing a space station on surface must fail, and at 400km LEO must succeed */
    TEST_ASSERT_FALSE(rts_hologlobe_validate_placement(&r->holo, &r->world, &r->eco, 1u, "stn_recon_sat", clear_site));
    uint32_t built_sat = rts_hologlobe_confirm_placement(&r->holo, &r->world, &r->eco, 1u, "stn_recon_sat",
                                                         rts_v3d(5000.0, r->world.planet.radius_m + 400000.0, 5000.0));
    TEST_ASSERT_TRUE(built_sat > 0u);
    TEST_ASSERT_TRUE(rts_world_get_entity_const(&r->world, built_sat)->orbital_mode);
    /* Non-building/non-station rejected */
    TEST_ASSERT_FALSE(rts_hologlobe_validate_placement(&r->holo, &r->world, &r->eco, 1u, "veh_plasma_tank", clear_site));
    /* Insufficient resources rejected */
    r->eco.ledgers[1].ore = 1.0;
    TEST_ASSERT_FALSE(rts_hologlobe_validate_placement(&r->holo, &r->world, &r->eco, 1u, "bldg_solar_geothermal",
                                                       rts_v3d(-320.0, r->world.planet.radius_m, 180.0)));

    destroy_rig(r);
}

static void test_tier1_field_gear_and_tier2_facility_consoles(void) {
    ToolkitTestRig *r = create_rig();

    /* Exercise tab names, radial order names, and optical zoom levels */
    for (int t = 0; t <= (int)RTS_WPAD_TAB_COUNT; ++t) TEST_ASSERT_NOT_NULL(rts_wrist_tab_name((RtsWristPadTab)t));
    for (int o = 0; o <= (int)RTS_RADIAL_COUNT; ++o) TEST_ASSERT_NOT_NULL(rts_radial_order_name((RtsRadialOrder)o));

    rts_field_gear_toggle_wrist_pad(&r->gear, &r->world);
    TEST_ASSERT_TRUE(r->gear.wrist_pad_open && r->world.commander.wrist_pad_open);
    rts_field_gear_cycle_wrist_tab(&r->gear, 1);
    TEST_ASSERT_EQUAL_INT(RTS_WPAD_DECK_MAP, r->gear.wrist_tab);
    rts_field_gear_cycle_wrist_tab(&r->gear, -2);
    TEST_ASSERT_EQUAL_INT(RTS_WPAD_ECONOMY, r->gear.wrist_tab);

    /* Optical Binoculars 1x -> 4x -> 12x -> 1x */
    TEST_ASSERT_EQUAL_INT(1, rts_field_gear_optics_zoom_x(&r->gear));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 62.0f, rts_field_gear_fov_deg(&r->gear));
    TEST_ASSERT_EQUAL_INT(4, rts_field_gear_cycle_optics(&r->gear));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 20.0f, rts_field_gear_fov_deg(&r->gear));
    TEST_ASSERT_EQUAL_INT(12, rts_field_gear_cycle_optics(&r->gear));
    TEST_ASSERT_FLOAT_WITHIN(0.1f, 7.5f, rts_field_gear_fov_deg(&r->gear));
    TEST_ASSERT_EQUAL_INT(1, rts_field_gear_cycle_optics(&r->gear));

    /* Radar scan obeying spherical-horizon Fog of War */
    RtsRadarContact contacts[RTS_RADAR_MAX_CONTACTS];
    bool comms_ok = false;
    uint32_t cit_id = find_bp(&r->world, "bldg_command_citadel", 1u);
    size_t n = rts_field_gear_scan_radar(&r->world, 1u, cit_id, rts_v3d(0.0, r->world.planet.radius_m, 0.0),
                                         450.0, contacts, RTS_RADAR_MAX_CONTACTS, &comms_ok);
    TEST_ASSERT_TRUE(n >= 4u && comms_ok);

    /* Optical Laser Designator painting enemy tank, surface terrain, and open sky vector */
    uint32_t enemy_id = find_bp(&r->world, "veh_plasma_tank", 2u);
    const RtsEntity *enemy = rts_world_get_entity_const(&r->world, enemy_id);
    RtsVec3d from = rts_v3d(enemy->pos.x - 90.0, r->world.planet.radius_m + 3.0, enemy->pos.z);
    RtsRay eray = {from, rts_v3d_norm(rts_v3d_sub(enemy->pos, from))};
    TEST_ASSERT_TRUE(rts_field_gear_paint_target(&r->gear, &r->world, 1u, eray));
    TEST_ASSERT_EQUAL_UINT32(enemy_id, r->gear.painted_entity_id);
    TEST_ASSERT_TRUE(rts_field_gear_designate_strike(&r->gear, &r->world, 1u) >= 1u);

    /* Paint planetary surface */
    RtsRay ground_ray = {rts_v3d(100.0, r->world.planet.radius_m + 50.0, 100.0), rts_v3d(0.0, -1.0, 0.0)};
    TEST_ASSERT_TRUE(rts_field_gear_paint_target(&r->gear, &r->world, 1u, ground_ray));
    TEST_ASSERT_EQUAL_UINT32(0u, r->gear.painted_entity_id);
    TEST_ASSERT_TRUE(rts_field_gear_designate_strike(&r->gear, &r->world, 1u) >= 1u);

    /* Paint open sky vector */
    RtsRay sky_ray = {rts_v3d(0.0, r->world.planet.radius_m + 50.0, 0.0), rts_v3d(0.0, 1.0, 0.0)};
    TEST_ASSERT_TRUE(rts_field_gear_paint_target(&r->gear, &r->world, 1u, sky_ray));

    /* Radial Quick Order Wheel execution across all 7 orders */
    rts_field_gear_toggle_radial(&r->gear);
    TEST_ASSERT_TRUE(r->gear.radial_open);
    RtsVec3d cpos = rts_v3d(0.0, r->world.planet.radius_m, 0.0);
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_ESCORT, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_MOVE, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_ATTACK, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_ORBIT, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_DOCK, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_BOARD, cpos));
    TEST_ASSERT_TRUE(rts_field_gear_exec_radial(&r->gear, &r->world, &r->ctrl, &r->boarding, &r->holo, 1u, RTS_RADIAL_EVACUATE, cpos));

    /* Tier 2 Interactive Facility Build Terminals (Foundry -> Vehicles, Shipyard -> Spacecraft, Carrier -> Aircraft, Naval Dock -> Naval) */
    uint32_t foundry_id = find_bp(&r->world, "bldg_vehicle_foundry", 1u);
    uint32_t shipyard_id = find_bp(&r->world, "stn_orbital_shipyard", 1u);
    TEST_ASSERT_TRUE(rts_facility_open_for_entity(&r->gear, &r->world, foundry_id));
    TEST_ASSERT_EQUAL_INT(RTS_CLASS_GROUND_VEHICLE, r->gear.facility_output_class);
    TEST_ASSERT_EQUAL_UINT32(10u, (uint32_t)r->gear.facility_item_count);
    uint32_t job_id = rts_facility_queue_index(&r->gear, &r->eco, &r->world, 0);
    TEST_ASSERT_TRUE(job_id > 0u);
    /* Invalid index rejected */
    TEST_ASSERT_EQUAL_UINT32(0u, rts_facility_queue_index(&r->gear, &r->eco, &r->world, 99));
    rts_facility_close(&r->gear);
    TEST_ASSERT_FALSE(r->gear.facility_open);

    TEST_ASSERT_TRUE(rts_facility_open_for_entity(&r->gear, &r->world, shipyard_id));
    TEST_ASSERT_EQUAL_INT(RTS_CLASS_SPACECRAFT, r->gear.facility_output_class);

    uint32_t car_id = rts_world_spawn_from_blueprint(&r->world, "ship_fleet_carrier", "Carrier", 1u,
                                                     r->world.planet_frame_id, rts_v3d(180, r->world.planet.radius_m, -140), true);
    TEST_ASSERT_TRUE(rts_facility_open_for_entity(&r->gear, &r->world, car_id));
    TEST_ASSERT_EQUAL_INT(RTS_CLASS_AIRCRAFT, r->gear.facility_output_class);

    uint32_t dock_id = rts_world_spawn_from_blueprint(&r->world, "bldg_coastal_dock", "Coastal Dock", 1u,
                                                      r->world.planet_frame_id, rts_v3d(220, r->world.planet.radius_m, -140), true);
    TEST_ASSERT_TRUE(rts_facility_open_for_entity(&r->gear, &r->world, dock_id));
    TEST_ASSERT_EQUAL_INT(RTS_CLASS_NAVAL_SHIP, r->gear.facility_output_class);

    destroy_rig(r);
}

static void test_renderer_rts_toolkit_all_tiers_and_gantry_assembly(void) {
    ToolkitTestRig *r = create_rig();

    uint32_t cit_id = find_bp(&r->world, "bldg_command_citadel", 1u);
    uint32_t foundry_id = find_bp(&r->world, "bldg_vehicle_foundry", 1u);
    uint32_t tank_id = find_bp(&r->world, "veh_plasma_tank", 1u);
    uint32_t enemy_id = find_bp(&r->world, "veh_plasma_tank", 2u);
    const RtsConstructBlueprint *cit_bp = rts_world_find_blueprint(&r->world, "bldg_command_citadel");

    TEST_ASSERT_TRUE(rts_mega_generate_for_blueprint(&r->mega, cit_bp, cit_id));
    TEST_ASSERT_TRUE(rts_mega_switch_deck(&r->mega, &r->world, 0u));
    rts_crew_populate_deck(&r->crew, &r->mega, &r->world);
    rts_renderer_set_active_interior(&r->rr, &r->mega, &r->crew);
    rts_renderer_set_rts_toolkit(&r->rr, &r->holo, &r->gear, &r->eco);

    /* Render Tier 3 Interactive 3D Holo-Globe with selected blip, attack arc, and valid & invalid building ghost */
    rts_hologlobe_select_entity(&r->holo, tank_id, false);
    rts_world_issue_order(&r->world, tank_id, RTS_ORDER_ATTACK, rts_world_get_entity_const(&r->world, enemy_id)->pos, enemy_id, 0.0);
    r->holo.mode = RTS_HOLO_MODE_PLACE_BUILDING;
    snprintf(r->holo.place_blueprint_id, sizeof(r->holo.place_blueprint_id), "bldg_plasma_turret");
    rts_hologlobe_validate_placement(&r->holo, &r->world, &r->eco, 1u, "bldg_plasma_turret", rts_v3d(-80, r->world.planet.radius_m, 40));
    r->world.commander.at_holo_globe = true;
    rts_renderer_render_live_world(&r->rr, &r->world, &r->input, 1.0f);

    /* Render all 4 Wrist-Pad tabs + Optics + Laser Beacon + Radial Wheel + Facility Console + 1:1 Gantry Assembly */
    r->world.commander.at_holo_globe = false;
    rts_facility_open_for_entity(&r->gear, &r->world, foundry_id);
    TEST_ASSERT_TRUE(rts_facility_queue_index(&r->gear, &r->eco, &r->world, 0) > 0u);
    r->gear.wrist_pad_open = true;
    r->gear.optics_level = 1;
    r->gear.radial_open = true;
    r->gear.paint_active = true;
    r->gear.painted_world_pos = rts_world_get_entity_const(&r->world, enemy_id)->pos;

    for (int t = 0; t < RTS_WPAD_TAB_COUNT; ++t) {
        r->gear.wrist_tab = (RtsWristPadTab)t;
        rts_renderer_render_live_world(&r->rr, &r->world, &r->input, 2.0f + (float)t);
    }

    RtsGfxStats st = r->rr.gfx.get_stats(r->rr.gfx.state);
    TEST_ASSERT_TRUE(st.draw_calls > 250u);
    TEST_ASSERT_EQUAL_UINT32(0u, st.gl_errors);

    destroy_rig(r);
}

void register_rts_toolkit_tests(void) {
    RUN_TEST(test_hologlobe_raycast_fow_orders_and_3d_placement);
    RUN_TEST(test_tier1_field_gear_and_tier2_facility_consoles);
    RUN_TEST(test_renderer_rts_toolkit_all_tiers_and_gantry_assembly);
}
