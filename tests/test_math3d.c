/* SPDX-License-Identifier: MIT */
#include "rts/math3d.h"
#include "unity.h"
#include <math.h>

static void test_vec3d_ops_and_floating_origin(void) {
    RtsVec3d a = rts_v3d(3.0, 4.0, 0.0);
    RtsVec3d b = rts_v3d(1.0, 2.0, 3.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, rts_v3d_len(a));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 11.0, rts_v3d_dot(a, b));
    RtsVec3d c = rts_v3d_cross(rts_v3d(1, 0, 0), rts_v3d(0, 1, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, c.z);
    RtsVec3d z = rts_v3d_norm(rts_v3d(0, 0, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rts_v3d_len(z));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 5.0, rts_v3d_dist(rts_v3d(0, 0, 0), a));

    /* Floating origin precision at 6,371 km planetary scale */
    RtsVec3d cam = rts_v3d(6371000.0, 0.0, 0.0);
    RtsVec3d obj = rts_v3d(6371002.5, 1.5, -3.0);
    RtsVec3f rel = rts_v3d_relative_f(obj, cam);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.5f, rel.x);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.5f, rel.y);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, -3.0f, rel.z);
}

static void test_quat_and_matrices(void) {
    RtsQuatd id = rts_quat_identity();
    RtsQuatd qz = rts_quat_from_axis_angle(rts_v3d(0, 0, 0), 1.0);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, qz.w);
    RtsQuatd qn0 = rts_quat_norm((RtsQuatd){0, 0, 0, 0});
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, qn0.w);

    RtsQuatd q = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), RTS_PI * 0.5);
    RtsVec3d v = rts_quat_rotate(q, rts_v3d(1, 0, 0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, v.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, -1.0, v.z);
    RtsQuatd inv = rts_quat_mul(q, rts_quat_conj(q));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 1.0, inv.w);

    RtsMat4f trs = rts_mat4_from_quat_trs(id, (RtsVec3f){2, 3, 4}, (RtsVec3f){2, 2, 2});
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 2.0f, trs.m[0]);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 4.0f, trs.m[14]);
    RtsMat4f mul = rts_mat4_mul(rts_mat4_translate((RtsVec3f){1, 0, 0}), rts_mat4_scale((RtsVec3f){2, 2, 2}));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, mul.m[12]);

    RtsMat4f p = rts_mat4_perspective(1.0f, 1.5f, 0.1f, 100.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, p.m[11]);
    RtsMat4f p0 = rts_mat4_perspective(1.0f, 0.0f, 0.1f, 100.0f);
    TEST_ASSERT_TRUE(p0.m[0] > 0.0f);
    RtsMat4f o = rts_mat4_ortho(0, 1280, 720, 0, -1, 1);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -1.0f, o.m[12]);
    RtsMat4f la = rts_mat4_look_at((RtsVec3f){0, 0, 5}, (RtsVec3f){0, 0, 0}, (RtsVec3f){0, 1, 0});
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, -5.0f, la.m[14]);
}

static void test_raycasts_and_clamps(void) {
    RtsAabb box = {rts_v3d(-1, -1, -1), rts_v3d(1, 1, 1)};
    TEST_ASSERT_TRUE(rts_aabb_contains(box, rts_v3d(0, 0, 0)));
    TEST_ASSERT_FALSE(rts_aabb_contains(box, rts_v3d(2, 0, 0)));

    double t = 0.0;
    RtsRay r_hit = {rts_v3d(0, 0, 5), rts_v3d(0, 0, -1)};
    TEST_ASSERT_TRUE(rts_ray_intersect_aabb(r_hit, box, &t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, t);

    RtsRay r_par_miss = {rts_v3d(5, 0, 5), rts_v3d(0, 0, -1)};
    TEST_ASSERT_FALSE(rts_ray_intersect_aabb(r_par_miss, box, NULL));
    RtsRay r_miss = {rts_v3d(0, 5, 5), rts_v3d(1, 0, -1)};
    TEST_ASSERT_FALSE(rts_ray_intersect_aabb(r_miss, box, NULL));

    TEST_ASSERT_TRUE(rts_ray_intersect_sphere(r_hit, rts_v3d(0, 0, 0), 1.0, &t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 4.0, t);
    /* Ray starting inside sphere */
    RtsRay r_in = {rts_v3d(0, 0, 0), rts_v3d(0, 0, -1)};
    TEST_ASSERT_TRUE(rts_ray_intersect_sphere(r_in, rts_v3d(0, 0, 0), 2.0, &t));
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 2.0, t);
    /* Ray behind sphere & invalid sphere */
    RtsRay r_behind = {rts_v3d(0, 0, -5), rts_v3d(0, 0, -1)};
    TEST_ASSERT_FALSE(rts_ray_intersect_sphere(r_behind, rts_v3d(0, 0, 0), 1.0, NULL));
    TEST_ASSERT_FALSE(rts_ray_intersect_sphere(r_par_miss, rts_v3d(0, 0, 0), 1.0, NULL));
    TEST_ASSERT_FALSE(rts_ray_intersect_sphere((RtsRay){rts_v3d(0,0,0), rts_v3d(0,0,0)}, rts_v3d(0,0,0), 1.0, NULL));

    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.0, rts_clampd(-2.0, 0.0, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 1.0, rts_clampd(2.0, 0.0, 1.0));
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, 0.5, rts_clampd(0.5, 0.0, 1.0));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, rts_clampf(-2.0f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, rts_clampf(2.0f, 0.0f, 1.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, rts_clampf(0.5f, 0.0f, 1.0f));
}

void register_math3d_tests(void) {
    RUN_TEST(test_vec3d_ops_and_floating_origin);
    RUN_TEST(test_quat_and_matrices);
    RUN_TEST(test_raycasts_and_clamps);
}
