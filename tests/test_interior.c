/* SPDX-License-Identifier: MIT */
#include "rts/interior.h"
#include "unity.h"

static void test_nested_frames_and_transforms(void) {
    RtsInteriorSystem sys;
    rts_interior_init(&sys);

    RtsFrameId f_planet = rts_interior_create_frame(&sys, RTS_FRAME_INVALID, "Planet", rts_v3d(0, 0, 0), rts_quat_identity(), RTS_GRAV_PLANETARY);
    RtsFrameId f_station = rts_interior_create_frame(&sys, f_planet, "Shipyard", rts_v3d(1000.0, 0.0, 0.0), rts_quat_from_axis_angle(rts_v3d(0, 1, 0), RTS_PI * 0.5), RTS_GRAV_ARTIFICIAL);
    RtsFrameId f_dropship = rts_interior_create_frame(&sys, f_station, "Dropship", rts_v3d(10.0, 5.0, 0.0), rts_quat_identity(), RTS_GRAV_ZERO_G);
    TEST_ASSERT_TRUE(f_dropship != RTS_FRAME_INVALID);
    TEST_ASSERT_EQUAL_UINT32(RTS_FRAME_INVALID, rts_interior_create_frame(&sys, 999u, "Bad", rts_v3d(0,0,0), rts_quat_identity(), RTS_GRAV_PLANETARY));

    RtsVec3d wp, lp;
    TEST_ASSERT_TRUE(rts_interior_local_to_world(&sys, f_dropship, rts_v3d(2.0, 0.0, 0.0), &wp));
    TEST_ASSERT_TRUE(rts_interior_world_to_local(&sys, f_dropship, wp, &lp));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, lp.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, lp.y);
    TEST_ASSERT_FALSE(rts_interior_local_to_world(&sys, 999u, rts_v3d(0,0,0), &wp));
    TEST_ASSERT_FALSE(rts_interior_world_to_local(&sys, 999u, wp, &lp));

    /* Reparent dropship back to planet & verify cycle rejection */
    TEST_ASSERT_FALSE(rts_interior_reparent_frame(&sys, f_station, f_dropship, rts_v3d(0,0,0), rts_quat_identity()));
    TEST_ASSERT_FALSE(rts_interior_reparent_frame(&sys, f_station, f_station, rts_v3d(0,0,0), rts_quat_identity()));
    TEST_ASSERT_FALSE(rts_interior_reparent_frame(&sys, f_station, 999u, rts_v3d(0,0,0), rts_quat_identity()));
    TEST_ASSERT_TRUE(rts_interior_reparent_frame(&sys, f_dropship, f_planet, rts_v3d(50.0, 0.0, 0.0), rts_quat_identity()));
}

static void test_rooms_portals_consoles_and_lod(void) {
    RtsInteriorSystem sys;
    rts_interior_init(&sys);
    RtsFrameId f1 = rts_interior_create_frame(&sys, RTS_FRAME_INVALID, "Citadel", rts_v3d(0, 0, 0), rts_quat_identity(), RTS_GRAV_PLANETARY);
    RtsFrameId f2 = rts_interior_create_frame(&sys, f1, "Dropship", rts_v3d(25.0, 0.0, 0.0), rts_quat_identity(), RTS_GRAV_ARTIFICIAL);
    RtsFrameId f3 = rts_interior_create_frame(&sys, f1, "DistantStation", rts_v3d(50000.0, 0.0, 0.0), rts_quat_identity(), RTS_GRAV_ZERO_G);

    RtsAabb room_box = {rts_v3d(-10, 0, -10), rts_v3d(10, 5, 10)};
    TEST_ASSERT_TRUE(rts_interior_add_room(&sys, f1, "War Room", room_box, (RtsVec3f){0.2f, 0.8f, 1.0f}, true) > 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, rts_interior_add_room(&sys, 999u, "Bad", room_box, (RtsVec3f){1,1,1}, true));
    TEST_ASSERT_NOT_NULL(rts_interior_find_room_at(&sys, f1, rts_v3d(0, 1.7, 0)));
    TEST_ASSERT_NULL(rts_interior_find_room_at(&sys, f1, rts_v3d(99, 1.7, 0)));

    RtsAabb pbox = {rts_v3d(8, 0, -2), rts_v3d(12, 4, 2)};
    TEST_ASSERT_TRUE(rts_interior_add_portal(&sys, f1, f2, "Hangar Ramp", pbox, rts_v3d(0, 1.7, 0), true) > 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, rts_interior_add_portal(&sys, f1, 999u, "Bad", pbox, rts_v3d(0,0,0), true));

    RtsFrameId cur_f = f1;
    RtsVec3d cur_p = rts_v3d(10.0, 1.5, 0.0);
    RtsVec3d cur_v = rts_v3d(2.0, 0.0, 0.0);
    TEST_ASSERT_TRUE(rts_interior_try_portal_transfer(&sys, &cur_f, &cur_p, &cur_v));
    TEST_ASSERT_EQUAL_UINT32(f2, cur_f);
    TEST_ASSERT_FALSE(rts_interior_try_portal_transfer(&sys, &cur_f, &cur_p, &cur_v));

    TEST_ASSERT_TRUE(rts_interior_add_console(&sys, f1, "3D Holo-Globe", RTS_CONSOLE_HOLO_GLOBE, rts_v3d(0, 1.0, 0), 3.0, 1u) > 0u);
    TEST_ASSERT_EQUAL_UINT32(0u, rts_interior_add_console(&sys, 999u, "Bad", RTS_CONSOLE_HOLO_GLOBE, rts_v3d(0,0,0), 0.0, 0u));
    const RtsConsoleStation *c = rts_interior_find_nearest_console(&sys, f1, rts_v3d(1.0, 1.0, 0.0));
    TEST_ASSERT_NOT_NULL(c);
    TEST_ASSERT_EQUAL_INT(RTS_CONSOLE_HOLO_GLOBE, c->type);
    TEST_ASSERT_NULL(rts_interior_find_nearest_console(&sys, f1, rts_v3d(50.0, 1.0, 0.0)));

    rts_interior_update_lod(&sys, f2, rts_v3d(25.0, 0.0, 0.0), 500.0);
    TEST_ASSERT_EQUAL_INT(RTS_INTERIOR_LOD_HYDRATED, rts_interior_get_frame_const(&sys, f2)->lod);
    TEST_ASSERT_EQUAL_INT(RTS_INTERIOR_LOD_ABSTRACTED, rts_interior_get_frame_const(&sys, f3)->lod);
}

void register_interior_tests(void) {
    RUN_TEST(test_nested_frames_and_transforms);
    RUN_TEST(test_rooms_portals_consoles_and_lod);
}
