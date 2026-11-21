/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <stdio.h>

static void test_obj_and_png_io(void) {
    const char *obj =
        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
        "f 1//1 2//1 3//1\n"
        "f 1/1 2/2 3/3\n"
        "f 1 2 3\n";
    RtsObjMesh m;
    TEST_ASSERT_TRUE(rts_obj_load_memory(obj, (RtsVec3f){1, 1, 1}, &m));
    TEST_ASSERT_EQUAL_UINT(15u, m.vert_count);
    rts_obj_free(&m);
    TEST_ASSERT_FALSE(rts_obj_load_memory(NULL, (RtsVec3f){1, 1, 1}, &m));
    TEST_ASSERT_FALSE(rts_obj_load_memory("# empty", (RtsVec3f){1, 1, 1}, &m));

    /* PNG write + reload from memory */
    uint8_t px[16] = {255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    TEST_ASSERT_TRUE(rts_image_write_png("/tmp/rts_unit_test_px.png", 2, 2, 4, px));
    TEST_ASSERT_FALSE(rts_image_write_png(NULL, 2, 2, 4, px));

    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    uint8_t *bytes = NULL;
    size_t sz = 0;
    TEST_ASSERT_TRUE(rts_vfs_read_bytes(&vfs, "assets", "textures/hull_blue.png", &bytes, &sz));
    RtsImage img;
    TEST_ASSERT_TRUE(rts_image_load_png_memory(bytes, sz, &img));
    TEST_ASSERT_EQUAL_INT(64, img.width);
    TEST_ASSERT_EQUAL_INT(64, img.height);
    rts_image_free(&img);
    rts_vfs_free(bytes);

    uint8_t bad[4] = {1, 2, 3, 4};
    TEST_ASSERT_FALSE(rts_image_load_png_memory(bad, 4, &img));
    TEST_ASSERT_FALSE(rts_image_load_png_memory(NULL, 0, &img));
}

static void test_retro_renderer_all_scenes_on_null_backend(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    RtsWorld world;
    rts_world_init(&world, rts_planet_earthlike(42u), 123u);
    RtsInputRouter input;
    rts_input_init(&input, 1280.0f, 720.0f, false);

    RtsRetroRenderer rr;
    TEST_ASSERT_FALSE(rts_renderer_init(&rr, rts_gfx_null_create(), NULL, 1280, 720));
    TEST_ASSERT_TRUE(rts_renderer_init(&rr, rts_gfx_null_create(), &vfs, 1280, 720));

    /* Exercise all 5 showcase scenes + touch fallback + screenshot capture */
    for (int s = 0; s < RTS_SCENE_COUNT; ++s) {
        rts_renderer_render_scene(&rr, &world, (s == RTS_SCENE_MOBILE_TOUCH_HUD ? NULL : &input), (RtsShowcaseScene)s, 1.5f + (float)s);
    }
    TEST_ASSERT_TRUE(rts_renderer_capture_png(&rr, "/tmp/rts_null_shot.png"));
    TEST_ASSERT_FALSE(rts_renderer_capture_png(&rr, NULL));

    RtsGfxStats st = rr.gfx.get_stats(rr.gfx.state);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)RTS_SCENE_COUNT, st.frames_begun);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)RTS_SCENE_COUNT, st.frames_ended);
    TEST_ASSERT_TRUE(st.draw_calls > 100u);
    TEST_ASSERT_EQUAL_UINT32(1u, st.screenshots_saved);

    rr.gfx.destroy_mesh(rr.gfx.state, rr.mesh_cube);
    rts_renderer_shutdown(&rr);
}

void register_assets_and_renderer_tests(void) {
    RUN_TEST(test_obj_and_png_io);
    RUN_TEST(test_retro_renderer_all_scenes_on_null_backend);
}
