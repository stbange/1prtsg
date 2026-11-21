/* SPDX-License-Identifier: MIT */
#include "rts/boarding.h"
#include "unity.h"
#include <string.h>

static void add_cap_bp(RtsWorld *w, const char *id, uint32_t crew, bool pod) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    strncpy(bp.id, id, sizeof(bp.id) - 1);
    strncpy(bp.name, id, sizeof(bp.name) - 1);
    bp.construct_class = RTS_CLASS_SPACECRAFT;
    bp.max_hull = 18000;
    bp.max_shield = 12000;
    bp.crew_capacity = crew;
    bp.can_orbit = true;
    bp.has_escape_pod = pod;
    rts_world_register_blueprint(w, &bp);
}

static void test_multi_deck_boarding_capture_and_repel(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 77u);
    add_cap_bp(&w, "space_dreadnought", 24, true);
    add_cap_bp(&w, "space_corvette", 6, false);
    add_cap_bp(&w, "space_escape_pod", 1, false);

    uint32_t dread = rts_world_spawn_from_blueprint(&w, "space_dreadnought", "Crimson Dreadnought", 2, w.planet_frame_id, rts_v3d(6771000, 0, 0), true);
    uint32_t corv = rts_world_spawn_from_blueprint(&w, "space_corvette", "Crimson Corvette", 2, w.planet_frame_id, rts_v3d(6771500, 0, 0), true);

    RtsBoardingSystem bs;
    rts_boarding_init(&bs);

    /* Reject friendly boarding and zero-marine boarding */
    TEST_ASSERT_EQUAL_UINT32(0u, rts_boarding_start(&bs, &w, dread, 2, 10, true));
    TEST_ASSERT_EQUAL_UINT32(0u, rts_boarding_start(&bs, &w, dread, 1, 0, true));

    /* Launch 28 mag-booted Solar Marines against Dreadnought with hangar decompression */
    uint32_t op1 = rts_boarding_start(&bs, &w, dread, 1, 28, true);
    TEST_ASSERT_TRUE(op1 > 0u);
    TEST_ASSERT_TRUE(rts_boarding_set_decompression(&bs, op1, true));
    TEST_ASSERT_FALSE(rts_boarding_set_decompression(&bs, 999u, true));
    TEST_ASSERT_NULL(rts_boarding_get_op(&bs, 999u));

    /* Launch undersized unmag-booted squad onto Corvette -> decompressed vacuum repels them */
    uint32_t op2 = rts_boarding_start(&bs, &w, corv, 1, 2, false);
    TEST_ASSERT_TRUE(op2 > 0u);
    TEST_ASSERT_TRUE(rts_boarding_set_decompression(&bs, op2, true));

    size_t done = 0;
    for (int i = 0; i < 30; ++i) {
        done += rts_boarding_step(&bs, &w, 1.0);
    }
    TEST_ASSERT_TRUE(done >= 2u);

    const RtsBoardingOp *r1 = rts_boarding_get_op(&bs, op1);
    const RtsBoardingOp *r2 = rts_boarding_get_op(&bs, op2);
    TEST_ASSERT_NOT_NULL(r1);
    TEST_ASSERT_NOT_NULL(r2);
    TEST_ASSERT_EQUAL_INT(RTS_BOARD_CAPTURED, r1->status);
    TEST_ASSERT_EQUAL_INT(RTS_DECK_BRIDGE_CIC, r1->current_deck);
    /* Dreadnought flipped to faction 1 intact! */
    TEST_ASSERT_EQUAL_UINT8(1u, rts_world_get_entity(&w, dread)->faction_id);
    TEST_ASSERT_EQUAL_INT(RTS_BOARD_REPELLED, r2->status);
    TEST_ASSERT_EQUAL_UINT8(2u, rts_world_get_entity(&w, corv)->faction_id);

    /* Boarding aborted cleanly if target destroyed mid-assault */
    uint32_t op3 = rts_boarding_start(&bs, &w, corv, 1, 12, true);
    rts_world_destroy_entity(&w, corv);
    rts_boarding_step(&bs, &w, 1.0);
    TEST_ASSERT_EQUAL_INT(RTS_BOARD_ABORTED_DESTROYED, rts_boarding_get_op(&bs, op3)->status);
}

static void test_reactor_meltdown_escape_pod_vs_ironman_defeat(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 88u);
    add_cap_bp(&w, "space_dreadnought", 20, true);
    add_cap_bp(&w, "space_corvette", 6, false);
    add_cap_bp(&w, "space_escape_pod", 1, false);

    uint32_t dread = rts_world_spawn_from_blueprint(&w, "space_dreadnought", "Solaris", 1, w.planet_frame_id, rts_v3d(6771000, 0, 0), true);
    uint32_t doomed = rts_world_spawn_from_blueprint(&w, "space_corvette", "Doomed", 1, w.planet_frame_id, rts_v3d(6772000, 0, 0), true);
    uint32_t temp_ship = rts_world_spawn_from_blueprint(&w, "space_corvette", "Temp", 1, w.planet_frame_id, rts_v3d(6773000, 0, 0), true);

    RtsBoardingSystem bs;
    rts_boarding_init(&bs);

    /* Reject bad meltdown params */
    TEST_ASSERT_FALSE(rts_boarding_trigger_meltdown(&bs, &w, 999u, 10.0));
    TEST_ASSERT_FALSE(rts_boarding_trigger_meltdown(&bs, &w, dread, 0.0));
    TEST_ASSERT_NULL(rts_boarding_get_meltdown(&bs, 999u));

    /* Commander boards Solaris -> Reactor Meltdown starts (8s) -> Duplicate rejected -> Ejects via Escape Pod at t=5s -> Survives! */
    w.commander.occupied_entity_id = dread;
    w.commander.frame_id = rts_world_get_entity(&w, dread)->own_frame_id;
    TEST_ASSERT_TRUE(rts_boarding_trigger_meltdown(&bs, &w, dread, 8.0));
    TEST_ASSERT_FALSE(rts_boarding_trigger_meltdown(&bs, &w, dread, 5.0));

    /* Also test pre-destroyed meltdown target exits gracefully */
    TEST_ASSERT_TRUE(rts_boarding_trigger_meltdown(&bs, &w, temp_ship, 10.0));
    rts_world_destroy_entity(&w, temp_ship);

    rts_boarding_step(&bs, &w, 3.0);
    TEST_ASSERT_TRUE(rts_world_commander_launch_escape_pod(&w));
    rts_boarding_step(&bs, &w, 6.0);
    TEST_ASSERT_TRUE(rts_boarding_get_meltdown(&bs, dread)->detonated);
    TEST_ASSERT_NULL(rts_world_get_entity(&w, dread));
    TEST_ASSERT_TRUE(w.commander.alive);
    TEST_ASSERT_EQUAL_INT(RTS_MATCH_ACTIVE, w.match_state);

    /* Now Commander boards Doomed Corvette without escape pod -> Meltdown triggers DEFEAT_IRONMAN */
    w.commander.occupied_entity_id = doomed;
    w.commander.frame_id = rts_world_get_entity(&w, doomed)->own_frame_id;
    w.commander.in_escape_pod = false;
    TEST_ASSERT_TRUE(rts_boarding_trigger_meltdown(&bs, &w, doomed, 4.0));
    rts_boarding_step(&bs, &w, 5.0);
    TEST_ASSERT_FALSE(w.commander.alive);
    TEST_ASSERT_EQUAL_INT(RTS_MATCH_DEFEAT_IRONMAN, w.match_state);
}

void register_boarding_tests(void) {
    RUN_TEST(test_multi_deck_boarding_capture_and_repel);
    RUN_TEST(test_reactor_meltdown_escape_pod_vs_ironman_defeat);
}
