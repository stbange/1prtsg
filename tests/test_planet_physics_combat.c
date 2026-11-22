/* SPDX-License-Identifier: MIT */
#include "rts/combat_sim.h"
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/interior_gen.h"
#include "rts/planet_mesh.h"
#include "rts/vehicle_physics.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void register_m011_blueprints(RtsWorld *w) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "veh_plasma_tank");
    snprintf(bp.name, sizeof(bp.name), "Plasma Heavy Tank");
    bp.construct_class = RTS_CLASS_GROUND_VEHICLE;
    bp.dimensions_m = (RtsVec3f){5.0f, 4.0f, 11.0f};
    bp.max_hull = 650.0f; bp.max_shield = 350.0f; bp.max_speed_mps = 24.0f;
    bp.crew_capacity = 3u; bp.room_count = 2u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/veh_plasma_tank.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "ship_attack_sub");
    snprintf(bp.name, sizeof(bp.name), "Attack Submarine");
    bp.construct_class = RTS_CLASS_NAVAL_SHIP;
    bp.dimensions_m = (RtsVec3f){12.0f, 18.0f, 110.0f};
    bp.max_hull = 1800.0f; bp.max_shield = 900.0f; bp.max_speed_mps = 18.0f;
    bp.crew_capacity = 26u; bp.room_count = 6u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/ship_attack_sub.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "ship_fleet_carrier");
    snprintf(bp.name, sizeof(bp.name), "Fleet Carrier");
    bp.construct_class = RTS_CLASS_NAVAL_SHIP;
    bp.dimensions_m = (RtsVec3f){78.0f, 58.0f, 340.0f};
    bp.max_hull = 7800.0f; bp.max_shield = 4600.0f; bp.max_speed_mps = 15.0f;
    bp.crew_capacity = 120u; bp.room_count = 16u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/ship_fleet_carrier.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "air_heavy_lifter");
    snprintf(bp.name, sizeof(bp.name), "Heavy Vehicle Lifter Dropship");
    bp.construct_class = RTS_CLASS_AIRCRAFT;
    bp.dimensions_m = (RtsVec3f){32.0f, 13.0f, 44.0f};
    bp.max_hull = 1600.0f; bp.max_shield = 900.0f; bp.max_speed_mps = 110.0f;
    bp.crew_capacity = 4u; bp.room_count = 3u; bp.can_orbit = true; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/air_heavy_lifter.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "spc_dreadnought");
    snprintf(bp.name, sizeof(bp.name), "Fleet Command Dreadnought");
    bp.construct_class = RTS_CLASS_SPACECRAFT;
    bp.dimensions_m = (RtsVec3f){140.0f, 95.0f, 680.0f};
    bp.max_hull = 12000.0f; bp.max_shield = 8000.0f; bp.max_speed_mps = 90.0f;
    bp.crew_capacity = 260u; bp.room_count = 20u; bp.can_orbit = true; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/spc_dreadnought.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "bldg_orbital_cannon");
    snprintf(bp.name, sizeof(bp.name), "Planetary Ion Orbital Cannon");
    bp.construct_class = RTS_CLASS_BUILDING;
    bp.dimensions_m = (RtsVec3f){68.0f, 95.0f, 68.0f};
    bp.max_hull = 3600.0f; bp.max_shield = 2000.0f; bp.max_speed_mps = 0.0f;
    bp.crew_capacity = 26u; bp.room_count = 6u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/bldg_orbital_cannon.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "bldg_flak_battery");
    snprintf(bp.name, sizeof(bp.name), "AA Flak Battery");
    bp.construct_class = RTS_CLASS_BUILDING;
    bp.dimensions_m = (RtsVec3f){26.0f, 18.0f, 26.0f};
    bp.max_hull = 1300.0f; bp.max_shield = 650.0f; bp.max_speed_mps = 0.0f;
    bp.crew_capacity = 6u; bp.room_count = 3u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/bldg_flak_battery.obj");
    rts_world_register_blueprint(w, &bp);

    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "veh_emp_crawler");
    snprintf(bp.name, sizeof(bp.name), "EMP Disruptor Crawler");
    bp.construct_class = RTS_CLASS_GROUND_VEHICLE;
    bp.dimensions_m = (RtsVec3f){8.0f, 6.0f, 16.0f};
    bp.max_hull = 720.0f; bp.max_shield = 900.0f; bp.max_speed_mps = 16.0f;
    bp.crew_capacity = 6u; bp.room_count = 2u; bp.active = true;
    snprintf(bp.mesh_path, sizeof(bp.mesh_path), "models/veh_shield_crawler.obj");
    rts_world_register_blueprint(w, &bp);
}

static void test_spherical_planet_mesh_lods_ocean_and_clouds(void) {
    RtsPlanetConfig planet = rts_planet_earthlike(42u);
    RtsPlanetLodSpec l0 = rts_planet_select_lod(120.0);
    RtsPlanetLodSpec l1 = rts_planet_select_lod(2500.0);
    RtsPlanetLodSpec l2 = rts_planet_select_lod(25000.0);
    RtsPlanetLodSpec l3 = rts_planet_select_lod(200000.0);
    RtsPlanetLodSpec l4 = rts_planet_select_lod(1200000.0);
    TEST_ASSERT_EQUAL_INT(0, l0.lod_level);
    TEST_ASSERT_EQUAL_INT(1, l1.lod_level);
    TEST_ASSERT_EQUAL_INT(2, l2.lod_level);
    TEST_ASSERT_EQUAL_INT(3, l3.lod_level);
    TEST_ASSERT_EQUAL_INT(4, l4.lod_level);
    TEST_ASSERT_TRUE(l1.render_clouds);
    TEST_ASSERT_FALSE(l4.render_ocean);

    /* Harbor bay opens to ocean at +X > 115m while base plateau at (0,R,0) is dry */
    double h_base = rts_planet_local_elevation(&planet, rts_v3d_norm(rts_v3d(0.0, planet.radius_m, 0.0)));
    double h_bay = rts_planet_local_elevation(&planet, rts_v3d_norm(rts_v3d(175.0, planet.radius_m, 20.0)));
    double h_ridge = rts_planet_local_elevation(&planet, rts_v3d_norm(rts_v3d(-190.0, planet.radius_m, -160.0)));
    double h_far = rts_planet_local_elevation(&planet, rts_v3d(1.0, 0.0, 0.0));
    TEST_ASSERT_TRUE(h_base > -2.0);
    TEST_ASSERT_TRUE(h_bay < 0.0);
    TEST_ASSERT_TRUE(rts_planet_local_is_ocean(&planet, rts_v3d_norm(rts_v3d(175.0, planet.radius_m, 20.0))));
    TEST_ASSERT_TRUE(h_ridge > 10.0);
    TEST_ASSERT_TRUE(fabs(h_far) <= planet.terrain_amp_m * 2.0);
    TEST_ASSERT_FALSE(rts_planet_local_is_ocean(NULL, rts_v3d(0, 1, 0)));

    RtsVec3d nrm = rts_planet_local_normal(&planet, rts_v3d(0.0, 1.0, 0.0), 8.0);
    TEST_ASSERT_TRUE(nrm.y > 0.7);

    RtsVertex *verts = (RtsVertex *)calloc(2400u, sizeof(RtsVertex));
    TEST_ASSERT_NOT_NULL(verts);
    size_t tcnt = rts_planet_build_surface_patch(&planet, rts_v3d(80.0, planet.radius_m + 18.0, 40.0), l0, verts, 2400u);
    size_t ocnt = rts_planet_build_ocean_patch(&planet, rts_v3d(80.0, planet.radius_m + 18.0, 40.0), l0, 1.5f, verts, 2400u);
    size_t ccnt = rts_planet_build_cloud_patch(&planet, rts_v3d(0.0, planet.radius_m + 24000.0, 0.0), 2.5f, verts, 2400u);
    TEST_ASSERT_TRUE(tcnt >= 600u);
    TEST_ASSERT_TRUE(ocnt >= 60u);
    TEST_ASSERT_TRUE(ccnt >= 300u);
    TEST_ASSERT_EQUAL_UINT(0u, rts_planet_build_ocean_patch(&planet, rts_v3d(0, planet.radius_m, 0), l4, 0.0f, verts, 2400u));
    free(verts);
}

static void test_six_class_vehicle_physics_cargo_and_time_warp(void) {
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    TEST_ASSERT_NOT_NULL(w);
    rts_world_init(w, rts_planet_earthlike(42u), 99u);
    register_m011_blueprints(w);
    double R = w->planet.radius_m;

    uint32_t tank = rts_world_spawn_from_blueprint(w, "veh_plasma_tank", "Tank 1", 1u, w->planet_frame_id, rts_v3d(0.0, R, 0.0), true);
    uint32_t sub = rts_world_spawn_from_blueprint(w, "ship_attack_sub", "Sub 1", 1u, w->planet_frame_id, rts_v3d(160.0, R, 20.0), true);
    uint32_t car = rts_world_spawn_from_blueprint(w, "ship_fleet_carrier", "Carrier 1", 1u, w->planet_frame_id, rts_v3d(185.0, R, 20.0), true);
    uint32_t lift = rts_world_spawn_from_blueprint(w, "air_heavy_lifter", "Lifter 1", 1u, w->planet_frame_id, rts_v3d(4.0, R + 3.0, 4.0), true);
    uint32_t drd = rts_world_spawn_from_blueprint(w, "spc_dreadnought", "Dreadnought 1", 1u, w->planet_frame_id, rts_v3d(0.0, R + 90000.0, 0.0), true);

    RtsVehiclePhysicsState vp;
    rts_vehicle_physics_init(&vp);
    RtsInputIntent in;
    memset(&in, 0, sizeof(in));
    in.move_y = 1.0f;

    /* Ground Vehicle */
    rts_vehicle_physics_bind(&vp, w, tank);
    for (int i = 0; i < 10; ++i) rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_TRUE(rts_world_get_entity_const(w, tank)->pos.z < -2.0);

    /* Naval Submarine dive + Surface Carrier buoyancy */
    rts_vehicle_physics_bind(&vp, w, sub);
    TEST_ASSERT_TRUE(vp.is_submarine);
    in.crouch_pressed = true;
    for (int i = 0; i < 20; ++i) rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_TRUE(vp.ballast_depth_m < -5.0);
    in.crouch_pressed = false;
    in.jump_pressed = true;
    for (int i = 0; i < 10; ++i) rts_vehicle_physics_step(&vp, w, &in, 0.1);
    rts_vehicle_physics_bind(&vp, w, car);
    TEST_ASSERT_FALSE(vp.is_submarine);
    rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 0.0, vp.ballast_depth_m);

    /* Aircraft / Dropship VTOL + Cargo Bay Clamping -> Ascent -> Release */
    rts_world_get_entity(w, tank)->pos = rts_v3d(4.0, R + 1.0, 4.0);
    rts_vehicle_physics_bind(&vp, w, lift);
    TEST_ASSERT_TRUE(rts_vehicle_clamp_cargo(&vp, w, lift, tank));
    in.jump_pressed = true;
    for (int i = 0; i < 15; ++i) rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_TRUE(rts_planet_altitude(&w->planet, rts_world_get_entity_const(w, tank)->pos) > 25.0);
    /* Too high to release safely */
    rts_world_get_entity(w, lift)->pos = rts_v3d(4.0, R + 90.0, 4.0);
    TEST_ASSERT_FALSE(rts_vehicle_release_cargo(&vp, w));
    /* Descend and release onto surface */
    rts_world_get_entity(w, lift)->pos = rts_v3d(4.0, R + 12.0, 4.0);
    TEST_ASSERT_TRUE(rts_vehicle_release_cargo(&vp, w));
    TEST_ASSERT_EQUAL_UINT32(0u, vp.clamped_cargo_id);

    /* Spacecraft 6-DOF orbital + re-entry heat flux + safe Time Compression */
    RtsEntity *de = rts_world_get_entity(w, drd);
    de->orbital_mode = true;
    de->vel = rts_v3d(0.0, -300.0, -7200.0);
    rts_vehicle_physics_bind(&vp, w, drd);
    rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_TRUE(vp.prograde_dv_mps > 6000.0);
    de->orbital_mode = false;
    de->pos = rts_v3d(0.0, R + 45000.0, 0.0);
    rts_vehicle_physics_step(&vp, w, &in, 0.1);
    TEST_ASSERT_TRUE(vp.reentry_heat_wm2 > 1000.0);

    TEST_ASSERT_EQUAL_INT(2, rts_time_compression_cycle(&vp, false));
    TEST_ASSERT_EQUAL_INT(4, rts_time_compression_cycle(&vp, false));
    TEST_ASSERT_EQUAL_INT(8, rts_time_compression_cycle(&vp, false));
    TEST_ASSERT_EQUAL_INT(1, rts_time_compression_cycle(&vp, false));
    TEST_ASSERT_EQUAL_INT(1, rts_time_compression_set(&vp, 8, true));
    TEST_ASSERT_TRUE(vp.time_warp_blocked_combat);

    free(w);
}

static void test_unified_live_combat_emp_compartments_voxel_and_renderer(void) {
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsCombatSim *cs = (RtsCombatSim *)calloc(1, sizeof(RtsCombatSim));
    RtsMegaInterior *mega = (RtsMegaInterior *)calloc(1, sizeof(RtsMegaInterior));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(cs);
    TEST_ASSERT_NOT_NULL(mega);

    rts_world_init(w, rts_planet_earthlike(42u), 123u);
    register_m011_blueprints(w);
    rts_combat_init(cs);
    double R = w->planet.radius_m;

    uint32_t ion = rts_world_spawn_from_blueprint(w, "bldg_orbital_cannon", "Ion 1", 1u, w->planet_frame_id, rts_v3d(-25.0, R, 0.0), false);
    uint32_t emp = rts_world_spawn_from_blueprint(w, "veh_emp_crawler", "EMP 1", 1u, w->planet_frame_id, rts_v3d(-15.0, R, 10.0), false);
    uint32_t flak = rts_world_spawn_from_blueprint(w, "bldg_flak_battery", "AA 1", 1u, w->planet_frame_id, rts_v3d(-15.0, R, -10.0), false);
    uint32_t ally = rts_world_spawn_from_blueprint(w, "veh_plasma_tank", "Vanguard 1", 1u, w->planet_frame_id, rts_v3d(-10.0, R, 0.0), false);
    uint32_t enemy = rts_world_spawn_from_blueprint(w, "spc_dreadnought", "Crimson Dread", 2u, w->planet_frame_id, rts_v3d(35.0, R, 0.0), true);
    uint32_t sub = rts_world_spawn_from_blueprint(w, "ship_attack_sub", "Enemy Sub", 2u, w->planet_frame_id, rts_v3d(55.0, R, 12.0), false);

    TEST_ASSERT_EQUAL_INT(RTS_WPN_ORBITAL_ION, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, ion)));
    TEST_ASSERT_EQUAL_INT(RTS_WPN_EMP_PULSE, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, emp)));
    TEST_ASSERT_EQUAL_INT(RTS_WPN_AA_FLAK, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, flak)));
    TEST_ASSERT_EQUAL_INT(RTS_WPN_PLASMA_BOLT, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, ally)));
    TEST_ASSERT_EQUAL_INT(RTS_WPN_BEAM_LANCE, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, enemy)));
    TEST_ASSERT_EQUAL_INT(RTS_WPN_HOMING_TORPEDO, rts_combat_weapon_for_entity(rts_world_get_entity_const(w, sub)));

    /* Compartment hitboxes map local offsets to distinct subsystems */
    const RtsEntity *ee = rts_world_get_entity_const(w, enemy);
    TEST_ASSERT_EQUAL_INT(RTS_SUBSYS_BRIDGE, rts_combat_hit_subsystem(ee, rts_v3d(ee->pos.x, ee->pos.y + 45.0, ee->pos.z)));
    TEST_ASSERT_EQUAL_INT(RTS_SUBSYS_ENGINES, rts_combat_hit_subsystem(ee, rts_v3d(ee->pos.x, ee->pos.y, ee->pos.z + 240.0)));
    TEST_ASSERT_EQUAL_INT(RTS_SUBSYS_WEAPONS, rts_combat_hit_subsystem(ee, rts_v3d(ee->pos.x, ee->pos.y, ee->pos.z - 240.0)));
    TEST_ASSERT_EQUAL_INT(RTS_SUBSYS_SENSORS, rts_combat_hit_subsystem(ee, rts_v3d(ee->pos.x + 65.0, ee->pos.y, ee->pos.z)));
    TEST_ASSERT_EQUAL_INT(RTS_SUBSYS_REACTOR, rts_combat_hit_subsystem(ee, ee->pos));

    /* Mega interior wired for compartment damage propagation */
    rts_mega_generate_for_blueprint(mega, rts_world_find_blueprint(w, "spc_dreadnought"), enemy);
    rts_combat_apply_emp_prime(cs, w, enemy, 5.0f);
    TEST_ASSERT_TRUE(rts_combat_is_emp_primed(cs, w, enemy));

    /* Strip shield so hull & subsystem take breach damage, trigger EMP x2.4 synergy + voxel crater */
    rts_world_get_entity(w, enemy)->shield = 0.0f;
    rts_world_get_entity(w, enemy)->subsystem_hp[RTS_SUBSYS_REACTOR] = 74.0f;
    TEST_ASSERT_TRUE(rts_combat_fire(cs, w, ion, enemy, rts_v3d(0, 0, 0), RTS_WPN_ORBITAL_ION));
    TEST_ASSERT_TRUE(rts_combat_fire(cs, w, ally, enemy, rts_v3d(0, 0, 0), RTS_WPN_PLASMA_BOLT));
    TEST_ASSERT_TRUE(rts_combat_fire(cs, w, sub, ally, rts_v3d(0, 0, 0), RTS_WPN_HOMING_TORPEDO));
    TEST_ASSERT_TRUE(rts_combat_fire(cs, w, emp, enemy, rts_v3d(0, 0, 0), RTS_WPN_EMP_PULSE));
    TEST_ASSERT_TRUE(rts_combat_fire(cs, w, flak, enemy, rts_v3d(0, 0, 0), RTS_WPN_AA_FLAK));
    TEST_ASSERT_TRUE(rts_combat_is_active(cs, w));

    for (int t = 0; t < 25; ++t) {
        rts_combat_step(cs, w, mega, 0.05);
    }
    TEST_ASSERT_TRUE(cs->emp_combo_hits >= 1u);
    TEST_ASSERT_TRUE(cs->total_hits >= 2u);
    TEST_ASSERT_TRUE(cs->crater_count >= 1u);
    TEST_ASSERT_TRUE(cs->subsystem_breaches >= 1u);
    TEST_ASSERT_TRUE(rts_combat_carve_voxel_crater(cs, rts_v3d(0.0, R, 0.0), 4.0f) >= 0);

    /* Render coverage pass across surface, cloud altitude, orbital view, combat VFX, and all vehicle telemetry modes */
    RtsVfs vfs;
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    RtsInputRouter inp;
    RtsVehiclePhysicsState vp;
    TEST_ASSERT_NOT_NULL(rr);
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    rts_input_init(&inp, 1280.0f, 720.0f, false);
    rts_vehicle_physics_init(&vp);
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1280, 720));
    rts_renderer_set_live_combat_physics(rr, cs, &vp);

    /* Surface render with combat VFX + Ground HUD */
    w->commander.frame_id = w->planet_frame_id;
    w->commander.local_pos = rts_v3d(0.0, R + 12.0, 28.0);
    rts_vehicle_physics_bind(&vp, w, ally);
    rts_renderer_render_live_world(rr, w, &inp, 1.0f);

    /* High altitude clouds + Aircraft HUD */
    w->commander.local_pos = rts_v3d(0.0, R + 4500.0, 0.0);
    rts_vehicle_physics_bind(&vp, w, rts_world_spawn_from_blueprint(w, "air_heavy_lifter", "L2", 1u, w->planet_frame_id, rts_v3d(0, R + 4500.0, -20.0), false));
    rts_renderer_render_live_world(rr, w, &inp, 2.0f);

    /* Naval Submarine HUD */
    rts_vehicle_physics_bind(&vp, w, sub);
    rts_renderer_render_live_world(rr, w, &inp, 2.5f);

    /* Orbital view + Re-entry plasma cone HUD */
    w->commander.local_pos = rts_v3d(0.0, R + 115000.0, 20.0);
    rts_vehicle_physics_bind(&vp, w, enemy);
    vp.reentry_glow = 0.8f;
    vp.reentry_heat_wm2 = 150000.0;
    rts_renderer_render_live_world(rr, w, &inp, 3.0f);

    /* Default time warp HUD */
    vp.entity_id = 0u;
    vp.reentry_glow = 0.0f;
    vp.time_compression = 4;
    rts_renderer_render_live_world(rr, w, &inp, 3.5f);

    TEST_ASSERT_TRUE(rr->gfx.get_stats(rr->gfx.state).draw_calls > 20u);
    rts_renderer_shutdown(rr);
    free(rr);
    free(mega);
    free(cs);
    free(w);
}

void register_planet_physics_combat_tests(void) {
    RUN_TEST(test_spherical_planet_mesh_lods_ocean_and_clouds);
    RUN_TEST(test_six_class_vehicle_physics_cargo_and_time_warp);
    RUN_TEST(test_unified_live_combat_emp_compartments_voxel_and_renderer);
}
