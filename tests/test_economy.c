/* SPDX-License-Identifier: MIT */
#include "rts/economy.h"
#include "unity.h"
#include <string.h>

static void add_bp(RtsWorld *w, const char *id, RtsConstructClass cls, float hull, float shield, bool orb) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    strncpy(bp.id, id, sizeof(bp.id) - 1);
    strncpy(bp.name, id, sizeof(bp.name) - 1);
    bp.construct_class = cls;
    bp.max_hull = hull;
    bp.max_shield = shield;
    bp.crew_capacity = 4;
    bp.can_orbit = orb;
    rts_world_register_blueprint(w, &bp);
}

static void test_economy_production_and_income(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 11u);
    add_bp(&w, "bldg_vehicle_foundry", RTS_CLASS_BUILDING, 3000, 1000, false);
    add_bp(&w, "bldg_ore_mine", RTS_CLASS_BUILDING, 1500, 400, false);
    add_bp(&w, "bldg_fuel_plant", RTS_CLASS_BUILDING, 1800, 600, false);
    add_bp(&w, "stn_orbital_shipyard", RTS_CLASS_SPACE_STATION, 20000, 10000, true);
    add_bp(&w, "veh_plasma_tank", RTS_CLASS_GROUND_VEHICLE, 600, 300, false);
    add_bp(&w, "space_frigate", RTS_CLASS_SPACECRAFT, 3000, 2000, true);

    RtsEconomySystem eco;
    rts_economy_init(&eco);
    rts_economy_add_resources(&eco, 1, 500.0, 200.0, 100.0, 50u);

    uint32_t foundry = rts_world_spawn_from_blueprint(&w, "bldg_vehicle_foundry", "Foundry", 1, w.planet_frame_id, rts_v3d(0, 6371000, 0), true);
    rts_world_spawn_from_blueprint(&w, "bldg_ore_mine", "Mine", 1, w.planet_frame_id, rts_v3d(50, 6371000, 0), false);
    rts_world_spawn_from_blueprint(&w, "bldg_fuel_plant", "Fuel", 1, w.planet_frame_id, rts_v3d(80, 6371000, 0), false);
    uint32_t yard = rts_world_spawn_from_blueprint(&w, "stn_orbital_shipyard", "Yard", 1, w.planet_frame_id, rts_v3d(6771000, 0, 0), true);
    rts_world_get_entity(&w, yard)->orbital_mode = true;

    double ore_before = eco.ledgers[1].ore;
    uint32_t j1 = rts_economy_queue_build(&eco, &w, foundry, "veh_plasma_tank");
    TEST_ASSERT_TRUE(j1 > 0u);
    TEST_ASSERT_TRUE(eco.ledgers[1].ore < ore_before);
    TEST_ASSERT_EQUAL_UINT32(0u, rts_economy_queue_build(&eco, &w, 999u, "veh_plasma_tank"));
    TEST_ASSERT_EQUAL_UINT32(0u, rts_economy_queue_build(&eco, &w, foundry, "missing_bp"));

    /* Orbital shipyard build inherits orbital_mode */
    uint32_t j2 = rts_economy_queue_build(&eco, &w, yard, "space_frigate");
    TEST_ASSERT_TRUE(j2 > 0u);

    /* Destroyed producer cancels job cleanly */
    uint32_t dead_f = rts_world_spawn_from_blueprint(&w, "bldg_vehicle_foundry", "Temp", 1, w.planet_frame_id, rts_v3d(200, 6371000, 0), false);
    uint32_t j3 = rts_economy_queue_build(&eco, &w, dead_f, "veh_plasma_tank");
    rts_world_destroy_entity(&w, dead_f);

    /* Low-power slowdown path */
    eco.ledgers[1].power_consumed = 999.0;
    rts_economy_step(&eco, &w, 1.0);
    eco.ledgers[1].power_consumed = 100.0;

    size_t completed = 0;
    for (int i = 0; i < 40; ++i) {
        completed += rts_economy_step(&eco, &w, 1.0);
    }
    TEST_ASSERT_EQUAL_UINT(2u, completed);
    const RtsProductionJob *job1 = rts_economy_get_job(&eco, j1);
    const RtsProductionJob *job2 = rts_economy_get_job(&eco, j2);
    const RtsProductionJob *job3 = rts_economy_get_job(&eco, j3);
    TEST_ASSERT_TRUE(job1 && job1->completed && job1->spawned_entity_id > 0u);
    TEST_ASSERT_TRUE(job2 && job2->completed && rts_world_get_entity(&w, job2->spawned_entity_id)->orbital_mode);
    TEST_ASSERT_TRUE(job3 && !job3->active);
    TEST_ASSERT_NULL(rts_economy_get_job(&eco, 999u));

    /* Insufficient ore rejects queue */
    eco.ledgers[1].ore = 0.0;
    TEST_ASSERT_EQUAL_UINT32(0u, rts_economy_queue_build(&eco, &w, foundry, "veh_plasma_tank"));
}

static void test_spherical_horizon_sensors_and_comms_relays(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 22u);
    add_bp(&w, "bldg_command_citadel", RTS_CLASS_BUILDING, 5000, 2000, false);
    add_bp(&w, "bldg_radar_array", RTS_CLASS_BUILDING, 1500, 500, false);
    add_bp(&w, "bldg_comms_tower", RTS_CLASS_BUILDING, 1000, 400, false);
    add_bp(&w, "ship_relay_tender", RTS_CLASS_NAVAL_SHIP, 2400, 1200, false);
    add_bp(&w, "air_awacs_cruiser", RTS_CLASS_AIRCRAFT, 1800, 1000, false);
    add_bp(&w, "veh_apc", RTS_CLASS_GROUND_VEHICLE, 400, 200, false);
    add_bp(&w, "stn_recon_sat", RTS_CLASS_SPACE_STATION, 600, 400, true);
    add_bp(&w, "stn_comms_relay", RTS_CLASS_SPACE_STATION, 900, 600, true);

    double R = w.planet.radius_m;
    uint32_t hq = rts_world_spawn_from_blueprint(&w, "bldg_command_citadel", "HQ", 1, w.planet_frame_id, rts_v3d(0, R + 20, 0), true);
    uint32_t far_ally = rts_world_spawn_from_blueprint(&w, "veh_apc", "Far APC", 1, w.planet_frame_id, rts_v3d(R + 20, 0, 0), false);
    uint32_t far_enemy = rts_world_spawn_from_blueprint(&w, "bldg_command_citadel", "Enemy Pole", 2, w.planet_frame_id, rts_v3d(0, -R - 20, 0), false);

    /* Planet sphere blocks direct North-Pole to Equator & South-Pole LOS */
    TEST_ASSERT_FALSE(rts_planet_has_los(&w.planet, rts_v3d(0, R + 20, 0), rts_v3d(R + 20, 0, 0)));
    TEST_ASSERT_TRUE(rts_planet_has_los(NULL, rts_v3d(0, 0, 0), rts_v3d(1, 0, 0)));
    TEST_ASSERT_FALSE(rts_sensors_is_target_detected(&w, 1, far_enemy));
    TEST_ASSERT_TRUE(rts_sensors_is_target_detected(&w, 1, far_ally));
    TEST_ASSERT_FALSE(rts_comms_is_entity_linked(&w, hq, far_ally));
    TEST_ASSERT_TRUE(rts_comms_is_entity_linked(&w, hq, hq));
    TEST_ASSERT_FALSE(rts_comms_is_entity_linked(&w, hq, far_enemy));

    /* Spawn high Orbital Comms Relay Satellite at 45 deg high orbit -> bridges over-the-horizon link! */
    double r_geo = (R + 8500000.0) * 0.70710678;
    uint32_t relay = rts_world_spawn_from_blueprint(&w, "stn_comms_relay", "GEO Relay", 1, w.planet_frame_id, rts_v3d(r_geo, r_geo, 0), false);
    TEST_ASSERT_TRUE(rts_comms_is_entity_linked(&w, hq, far_ally));

    /* Destroying relay severs far-side comms link */
    rts_world_destroy_entity(&w, relay);
    TEST_ASSERT_FALSE(rts_comms_is_entity_linked(&w, hq, far_ally));

    /* Spawn Orbital Recon Satellite over enemy pole -> detects enemy! */
    uint32_t recon = rts_world_spawn_from_blueprint(&w, "stn_recon_sat", "Recon LEO", 1, w.planet_frame_id, rts_v3d(0, -R - 800000.0, 0), false);
    TEST_ASSERT_TRUE(rts_sensors_is_target_detected(&w, 1, far_enemy));
    rts_world_get_entity(&w, recon)->subsystem_hp[RTS_SUBSYS_SENSORS] = 0.0f;
    TEST_ASSERT_FALSE(rts_sensors_is_target_detected(&w, 1, far_enemy));

    /* Verify sensor/comms range tables for specialty hulls */
    uint32_t rad = rts_world_spawn_from_blueprint(&w, "bldg_radar_array", "Radar", 1, w.planet_frame_id, rts_v3d(0, R, 0), false);
    uint32_t awacs = rts_world_spawn_from_blueprint(&w, "air_awacs_cruiser", "AWACS", 1, w.planet_frame_id, rts_v3d(0, R + 5000, 0), false);
    uint32_t tower = rts_world_spawn_from_blueprint(&w, "bldg_comms_tower", "Tower", 1, w.planet_frame_id, rts_v3d(0, R, 0), false);
    uint32_t tender = rts_world_spawn_from_blueprint(&w, "ship_relay_tender", "Tender", 1, w.planet_frame_id, rts_v3d(0, R, 0), false);
    TEST_ASSERT_TRUE(rts_entity_sensor_range_m(rts_world_get_entity_const(&w, rad)) > 500000.0);
    TEST_ASSERT_TRUE(rts_entity_sensor_range_m(rts_world_get_entity_const(&w, awacs)) > 300000.0);
    TEST_ASSERT_TRUE(rts_entity_comms_range_m(rts_world_get_entity_const(&w, tower)) > 10000000.0);
    TEST_ASSERT_TRUE(rts_entity_comms_range_m(rts_world_get_entity_const(&w, tender)) > 5000000.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, rts_entity_sensor_range_m(NULL));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, rts_entity_comms_range_m(NULL));
}

void register_economy_tests(void) {
    RUN_TEST(test_economy_production_and_income);
    RUN_TEST(test_spherical_horizon_sensors_and_comms_relays);
}
