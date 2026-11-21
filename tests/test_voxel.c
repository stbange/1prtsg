/* SPDX-License-Identifier: MIT */
#include "rts/voxel.h"
#include "unity.h"

static void test_voxel_chunk_and_face_culling(void) {
    RtsVoxelChunk c;
    rts_voxel_init(&c, 1.0f);
    rts_voxel_set_palette(&c, 5, (RtsVec3f){0.9f, 0.3f, 0.1f});
    TEST_ASSERT_FALSE(rts_voxel_set(&c, -1, 0, 0, 1));
    TEST_ASSERT_EQUAL_UINT8(0u, rts_voxel_get(&c, 99, 0, 0));

    /* Single isolated voxel -> 6 faces * 6 verts = 36 verts */
    TEST_ASSERT_TRUE(rts_voxel_set(&c, 4, 4, 4, 1));
    RtsVoxelVertex buf[256];
    TEST_ASSERT_EQUAL_UINT(36u, rts_voxel_build_mesh(&c, buf, 256));

    /* Adjacent voxel -> shared internal face culled -> 10 faces * 6 = 60 verts */
    TEST_ASSERT_TRUE(rts_voxel_set(&c, 5, 4, 4, 5));
    TEST_ASSERT_EQUAL_UINT(60u, rts_voxel_build_mesh(&c, buf, 256));

    /* Box fill + small buffer truncation guard */
    rts_voxel_fill_box(&c, 1, 1, 1, 2, 2, 2, 2);
    TEST_ASSERT_EQUAL_UINT8(2u, rts_voxel_get(&c, 2, 2, 2));
    TEST_ASSERT_EQUAL_UINT(0u, rts_voxel_build_mesh(&c, buf, 3));

    /* Text loader */
    const char *txt = "# comment\n0 0 0 1\n1 0 0 2\n";
    TEST_ASSERT_TRUE(rts_voxel_load_text(&c, txt));
    TEST_ASSERT_EQUAL_UINT8(1u, rts_voxel_get(&c, 0, 0, 0));
    TEST_ASSERT_EQUAL_UINT8(2u, rts_voxel_get(&c, 1, 0, 0));
    TEST_ASSERT_FALSE(rts_voxel_load_text(&c, NULL));
    TEST_ASSERT_FALSE(rts_voxel_load_text(&c, "# empty\n"));
}

void register_voxel_tests(void) {
    RUN_TEST(test_voxel_chunk_and_face_culling);
}
