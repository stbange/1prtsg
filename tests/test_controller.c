/* SPDX-License-Identifier: MIT */
#include "rts/controller.h"
#include "unity.h"
#include <string.h>

static void add_craft_bp(RtsWorld *w, const char *id, RtsConstructClass cls, float spd, bool orb, bool pod) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    strncpy(bp.id, id, sizeof(bp.id) - 1);
    strncpy(bp.name, id, sizeof(bp.name) - 1);
    bp.construct_class = cls;
    bp.max_hull = 1000;
    bp.max_shield = 600;
    bp.max_speed_mps = spd;
    bp.crew_capacity = 8;
    bp.can_orbit = orb;
    bp.has_escape_pod = pod;
    rts_world_register_blueprint(w, &bp);
}

static void test_first_person_locomotion_consoles_and_piloting(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 33u);
    add_craft_bp(&w, "bldg_command_citadel", RTS_CLASS_BUILDING, 0.0f, false, true);
    add_craft_bp(&w, "air_dropship", RTS_CLASS_AIRCRAFT, 140.0f, true, true);
    add_craft_bp(&w, "space_escape_pod", RTS_CLASS_SPACECRAFT, 180.0f, true, false);

    uint32_t cit = rts_world_spawn_from_blueprint(&w, "bldg_command_citadel", "Citadel", 1, w.planet_frame_id, rts_v3d(0, 6371000, 0), true);
    uint32_t ds = rts_world_spawn_from_blueprint(&w, "air_dropship", "Dropship", 1, w.planet_frame_id, rts_v3d(30, 6371000, 0), true);
    RtsFrameId f_cit = rts_world_get_entity(&w, cit)->own_frame_id;
    rts_interior_add_room(&w.interiors, f_cit, "War Room", (RtsAabb){rts_v3d(-8, 0, -8), rts_v3d(8, 5, 8)}, (RtsVec3f){0.2f, 0.8f, 1.0f}, true);
    rts_interior_add_console(&w.interiors, f_cit, "Holo-Globe", RTS_CONSOLE_HOLO_GLOBE, rts_v3d(0, 1.5, -2.0), 3.0, cit);
    rts_interior_add_console(&w.interiors, f_cit, "Factory Term", RTS_CONSOLE_FACTORY_TERMINAL, rts_v3d(5.0, 1.5, 0.0), 2.5, cit);
    rts_interior_add_console(&w.interiors, f_cit, "Pilot Seat", RTS_CONSOLE_PILOT_SEAT, rts_v3d(-5.0, 1.5, 0.0), 2.5, ds);
    rts_interior_add_console(&w.interiors, f_cit, "Escape Pod", RTS_CONSOLE_ESCAPE_POD, rts_v3d(0.0, 1.5, 5.0), 2.5, cit);

    w.commander.frame_id = f_cit;
    w.commander.occupied_entity_id = cit;
    w.commander.local_pos = rts_v3d(0, 1.5, 2.0);

    RtsCommanderController ctrl;
    rts_controller_init(&ctrl);

    /* Walk forward (diagonal normalized + room clamp) & toggle pad + mag-boots */
    RtsInputIntent in;
    memset(&in, 0, sizeof(in));
    in.move_y = 1.0f;
    in.move_x = 1.0f;
    in.toggle_pad_pressed = true;
    in.mag_boot_pressed = true;
    rts_controller_step(&ctrl, &w, &in, 0.5);
    TEST_ASSERT_TRUE(w.commander.wrist_pad_open && w.commander.mag_boots);
    /* Clamp against wall */
    in.toggle_pad_pressed = false;
    in.mag_boot_pressed = false;
    for (int i = 0; i < 6; ++i) rts_controller_step(&ctrl, &w, &in, 0.5);
    TEST_ASSERT_TRUE(w.commander.local_pos.x <= 8.0 && w.commander.local_pos.z >= -8.0);

    /* Interact with Holo-Globe console, then exit */
    w.commander.local_pos = rts_v3d(0, 1.5, -1.5);
    memset(&in, 0, sizeof(in));
    in.interact_pressed = true;
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_CMD_AT_HOLO_GLOBE, ctrl.mode);
    TEST_ASSERT_TRUE(w.commander.at_holo_globe);
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_CMD_ON_FOOT, ctrl.mode);

    /* Interact with Factory console */
    w.commander.local_pos = rts_v3d(5.0, 1.5, 0.0);
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_CMD_AT_FACTORY, ctrl.mode);
    rts_controller_step(&ctrl, &w, &in, 0.1);

    /* Interact with Pilot Seat -> directly pilot Dropship -> release back to AI */
    w.commander.local_pos = rts_v3d(-5.0, 1.5, 0.0);
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_CMD_PILOTING, ctrl.mode);
    in.interact_pressed = false;
    in.move_y = 1.0f;
    double z_before = rts_world_get_entity(&w, ds)->pos.z;
    rts_controller_step(&ctrl, &w, &in, 0.5);
    TEST_ASSERT_TRUE(rts_world_get_entity(&w, ds)->pos.z < z_before);
    in.interact_pressed = true;
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_CMD_ON_FOOT, ctrl.mode);
    TEST_ASSERT_FALSE(rts_controller_engage_piloting(&ctrl, &w, cit)); /* static building cannot be piloted */

    /* Interact with Escape Pod console */
    w.commander.frame_id = f_cit;
    w.commander.occupied_entity_id = cit;
    w.commander.local_pos = rts_v3d(0.0, 1.5, 5.0);
    rts_controller_step(&ctrl, &w, &in, 0.1);
    TEST_ASSERT_TRUE(w.commander.in_escape_pod);
}

static void test_astrogator_ascent_rendezvous_and_shielded_reentry(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 44u);
    add_craft_bp(&w, "air_dropship", RTS_CLASS_AIRCRAFT, 140.0f, true, true);
    add_craft_bp(&w, "veh_plasma_tank", RTS_CLASS_GROUND_VEHICLE, 22.0f, false, false);
    add_craft_bp(&w, "stn_orbital_shipyard", RTS_CLASS_SPACE_STATION, 0.0f, true, true);

    uint32_t ds = rts_world_spawn_from_blueprint(&w, "air_dropship", "Valkyrie", 1, w.planet_frame_id, rts_v3d(0, 6371010, 0), true);
    uint32_t tank = rts_world_spawn_from_blueprint(&w, "veh_plasma_tank", "Tank", 1, w.planet_frame_id, rts_v3d(0, 6371000, 0), false);
    uint32_t stn = rts_world_spawn_from_blueprint(&w, "stn_orbital_shipyard", "Shipyard", 1, w.planet_frame_id, rts_v3d(0, 0, 0), true);
    RtsOrbitState st = rts_orbit_circular_state(&w.planet, 400000.0, 0.25, 0.002);
    RtsEntity *es = rts_world_get_entity(&w, stn);
    es->pos = st.pos_m;
    es->vel = st.vel_mps;
    es->orbital_mode = true;

    RtsCommanderController ctrl;
    rts_controller_init(&ctrl);

    /* Non-orbital tank rejected */
    TEST_ASSERT_FALSE(rts_astrogator_start_ascent(&ctrl, &w, tank, 400000.0));
    TEST_ASSERT_FALSE(rts_astrogator_start_ascent(&ctrl, &w, ds, 50000.0));

    /* Ascent to 400km circular orbit */
    TEST_ASSERT_TRUE(rts_astrogator_start_ascent(&ctrl, &w, ds, 400000.0));
    RtsAutopilotPhase ph = RTS_AUTO_IDLE;
    for (int i = 0; i < 80; ++i) {
        ph = rts_astrogator_step(&ctrl, &w, 1.0);
        if (ph == RTS_AUTO_CIRCULARIZED) break;
    }
    TEST_ASSERT_EQUAL_INT(RTS_AUTO_CIRCULARIZED, ph);
    TEST_ASSERT_TRUE(rts_world_get_entity(&w, ds)->orbital_mode);

    /* Orbital rendezvous & docking reparenting into Shipyard frame */
    TEST_ASSERT_TRUE(rts_astrogator_start_rendezvous(&ctrl, &w, ds, stn));
    for (int i = 0; i < 20 && !ctrl.astrogator.docked; ++i) {
        rts_astrogator_step(&ctrl, &w, 1.0);
    }
    TEST_ASSERT_TRUE(ctrl.astrogator.docked);
    TEST_ASSERT_EQUAL_UINT32(es->own_frame_id, rts_interior_get_frame(&w.interiors, rts_world_get_entity(&w, ds)->own_frame_id)->parent_id);

    /* Shielded atmospheric re-entry & landing */
    float sh_before = rts_world_get_entity(&w, ds)->shield;
    TEST_ASSERT_TRUE(rts_astrogator_start_reentry(&ctrl, &w, ds));
    for (int i = 0; i < 90; ++i) {
        ph = rts_astrogator_step(&ctrl, &w, 1.0);
        if (ph == RTS_AUTO_LANDED) break;
    }
    TEST_ASSERT_EQUAL_INT(RTS_AUTO_LANDED, ph);
    TEST_ASSERT_TRUE(ctrl.astrogator.peak_reentry_heat > 10.0);
    TEST_ASSERT_TRUE(rts_world_get_entity(&w, ds)->shield < sh_before);
}

void register_controller_tests(void) {
    RUN_TEST(test_first_person_locomotion_consoles_and_piloting);
    RUN_TEST(test_astrogator_ascent_rendezvous_and_shielded_reentry);
}
