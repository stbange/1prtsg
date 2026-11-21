/* SPDX-License-Identifier: MIT */
#include "rts/world.h"
#include "unity.h"
#include <string.h>

static RtsConstructBlueprint make_bp(const char *id, RtsConstructClass cls, float hull, float shield, float spd, bool pod) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    strncpy(bp.id, id, sizeof(bp.id) - 1);
    strncpy(bp.name, id, sizeof(bp.name) - 1);
    bp.construct_class = cls;
    bp.dimensions_m = (RtsVec3f){20, 10, 8};
    bp.max_hull = hull;
    bp.max_shield = shield;
    bp.max_speed_mps = spd;
    bp.crew_capacity = 12;
    bp.has_escape_pod = pod;
    return bp;
}

static void test_world_blueprints_orders_damage_and_repair(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(1u), 0u);
    double r1 = rts_world_rand01(&w);
    double r2 = rts_world_rand01(&w);
    TEST_ASSERT_TRUE(r1 >= 0.0 && r1 < 1.0 && r1 != r2);

    RtsConstructBlueprint tank_bp = make_bp("veh_plasma_tank", RTS_CLASS_GROUND_VEHICLE, 500, 200, 25, false);
    RtsConstructBlueprint pod_bp = make_bp("space_escape_pod", RTS_CLASS_SPACECRAFT, 200, 200, 100, false);
    RtsConstructBlueprint dread_bp = make_bp("space_command_dreadnought", RTS_CLASS_SPACECRAFT, 5000, 1000, 80, true);

    TEST_ASSERT_TRUE(rts_world_register_blueprint(&w, &tank_bp));
    /* Override same blueprint id cleanly */
    tank_bp.max_hull = 600;
    TEST_ASSERT_TRUE(rts_world_register_blueprint(&w, &tank_bp));
    TEST_ASSERT_TRUE(rts_world_register_blueprint(&w, &pod_bp));
    TEST_ASSERT_TRUE(rts_world_register_blueprint(&w, &dread_bp));
    TEST_ASSERT_FALSE(rts_world_register_blueprint(&w, NULL));

    RtsCrewRoleDef role;
    memset(&role, 0, sizeof(role));
    strncpy(role.id, "role_engineer", sizeof(role.id) - 1);
    TEST_ASSERT_TRUE(rts_world_register_crew_role(&w, &role));
    TEST_ASSERT_TRUE(rts_world_register_crew_role(&w, &role)); /* update path */

    uint32_t t1 = rts_world_spawn_from_blueprint(&w, "veh_plasma_tank", "Tank 1", 1, w.planet_frame_id, rts_v3d(0, 0, 0), true);
    uint32_t t2 = rts_world_spawn_from_blueprint(&w, "veh_plasma_tank", "Enemy 1", 2, w.planet_frame_id, rts_v3d(100, 0, 0), false);
    TEST_ASSERT_TRUE(t1 > 0u && t2 > 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, rts_world_spawn_from_blueprint(&w, "unknown", NULL, 1, 0, rts_v3d(0,0,0), false));
    TEST_ASSERT_NULL(rts_world_get_entity(&w, 999u));
    TEST_ASSERT_NULL(rts_world_get_entity_const(&w, 999u));

    /* Move order */
    TEST_ASSERT_TRUE(rts_world_issue_order(&w, t1, RTS_ORDER_MOVE, rts_v3d(40, 0, 0), 0, 0));
    TEST_ASSERT_FALSE(rts_world_issue_order(&w, 999u, RTS_ORDER_MOVE, rts_v3d(0,0,0), 0, 0));
    for (int i = 0; i < 30; ++i) rts_world_step(&w, 0.1);
    TEST_ASSERT_DOUBLE_WITHIN(0.5, 40.0, rts_world_get_entity(&w, t1)->pos.x);

    /* Attack order + shield flash + regen + localized damage + repair */
    TEST_ASSERT_TRUE(rts_world_issue_order(&w, t1, RTS_ORDER_ATTACK, rts_v3d(0,0,0), t2, 0));
    rts_world_step(&w, 0.5);
    TEST_ASSERT_TRUE(rts_world_get_entity(&w, t2)->shield < 200.0f);

    rts_world_apply_damage(&w, t1, 260.0f, RTS_SUBSYS_ENGINES);
    RtsEntity *e1 = rts_world_get_entity(&w, t1);
    TEST_ASSERT_TRUE(e1->subsystem_hp[RTS_SUBSYS_ENGINES] < 100.0f);
    TEST_ASSERT_TRUE(rts_world_effective_speed(e1) < e1->max_speed_mps);
    for (int i = 0; i < 20; ++i) rts_world_step(&w, 0.5);
    TEST_ASSERT_TRUE(e1->subsystem_hp[RTS_SUBSYS_ENGINES] > 90.0f);
    TEST_ASSERT_TRUE(e1->shield > 0.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, rts_world_effective_speed(NULL));

    /* Destroy target cancels attacker order */
    TEST_ASSERT_TRUE(rts_world_destroy_entity(&w, t2));
    TEST_ASSERT_FALSE(rts_world_destroy_entity(&w, t2));
    rts_world_step(&w, 0.1);
    TEST_ASSERT_EQUAL_INT(RTS_ORDER_NONE, rts_world_get_entity(&w, t1)->order_type);
}

static void test_ironman_escape_pod_orbital_and_crew_lod(void) {
    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(9u), 55u);
    RtsConstructBlueprint pod_bp = make_bp("space_escape_pod", RTS_CLASS_SPACECRAFT, 200, 200, 100, false);
    RtsConstructBlueprint dread_bp = make_bp("space_command_dreadnought", RTS_CLASS_SPACECRAFT, 1000, 200, 80, true);
    rts_world_register_blueprint(&w, &pod_bp);
    rts_world_register_blueprint(&w, &dread_bp);

    uint32_t d1 = rts_world_spawn_from_blueprint(&w, "space_command_dreadnought", "Solaris", 1, w.planet_frame_id, rts_v3d(6771000, 0, 0), true);
    RtsEntity *ed = rts_world_get_entity(&w, d1);
    RtsOrbitState st = rts_orbit_circular_state(&w.planet, 400000.0, 0.2, 0.0);
    ed->pos = st.pos_m;
    ed->vel = st.vel_mps;
    ed->orbital_mode = true;

    w.commander.occupied_entity_id = d1;
    w.commander.frame_id = ed->own_frame_id;
    rts_world_step(&w, 1.0);
    TEST_ASSERT_TRUE(rts_planet_altitude(&w.planet, ed->pos) > 399000.0);

    /* Escape pod saves commander when dreadnought is destroyed */
    TEST_ASSERT_TRUE(rts_world_commander_launch_escape_pod(&w));
    TEST_ASSERT_FALSE(rts_world_commander_launch_escape_pod(&w)); /* already in pod */
    rts_world_apply_damage(&w, d1, 5000.0f, RTS_SUBSYS_REACTOR);
    TEST_ASSERT_TRUE(w.commander.alive);
    TEST_ASSERT_EQUAL_INT(RTS_MATCH_ACTIVE, w.match_state);

    /* Direct lethal damage to commander triggers Hardcore Ironman defeat */
    rts_world_damage_commander(&w, 50.0f);
    TEST_ASSERT_TRUE(w.commander.alive);
    rts_world_damage_commander(&w, 500.0f);
    TEST_ASSERT_FALSE(w.commander.alive);
    TEST_ASSERT_EQUAL_INT(RTS_MATCH_DEFEAT_IRONMAN, w.match_state);

    /* Hybrid crew visual LOD */
    int dir = -1;
    TEST_ASSERT_EQUAL_INT(RTS_CREW_LOD_3D_MESH, rts_world_eval_crew_lod(120.0, true, false, 45.0, -0.5, 8, &dir));
    TEST_ASSERT_TRUE(dir >= 0 && dir < 8);
    TEST_ASSERT_EQUAL_INT(RTS_CREW_LOD_3D_MESH, rts_world_eval_crew_lod(120.0, false, true, 45.0, 1.0, 8, &dir));
    TEST_ASSERT_EQUAL_INT(RTS_CREW_LOD_3D_MESH, rts_world_eval_crew_lod(10.0, false, false, 45.0, 1.0, 8, &dir));
    TEST_ASSERT_EQUAL_INT(RTS_CREW_LOD_SPRITE,  rts_world_eval_crew_lod(90.0, false, false, 45.0, 3.14, 8, &dir));
}

void register_world_tests(void) {
    RUN_TEST(test_world_blueprints_orders_damage_and_repair);
    RUN_TEST(test_ironman_escape_pod_orbital_and_crew_lod);
}
