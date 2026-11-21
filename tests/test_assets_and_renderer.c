/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

static void test_obj_and_png_io(void) {
    const char *obj =
        "v 0 0 0 1.0 0.8 0.5\nv 1 0 0\nv 1 1 0 0.2 1.8 2.0\nv 0 1 0\n"
        "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "vn 0 0 1\n"
        "f 1/1/1 2/2/1 3/3/1 4/4/1\n"
        "f 1//1 2//1 3//1\n"
        "f 1/1 2/2 3/3\n"
        "f 1 2 3\n";
    RtsObjMesh m;
    TEST_ASSERT_TRUE(rts_obj_load_memory(obj, (RtsVec3f){1, 1, 1}, &m));
    TEST_ASSERT_EQUAL_UINT(15u, m.vert_count);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.8f, m.verts[0].g);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 2.0f, m.verts[2].b);
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
    TEST_ASSERT_EQUAL_INT(256, img.width);
    TEST_ASSERT_EQUAL_INT(256, img.height);
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
    RtsGfxBackend bad_gfx = rts_gfx_null_create();
    TEST_ASSERT_FALSE(rts_renderer_init(&rr, bad_gfx, NULL, 1280, 720));
    bad_gfx.shutdown(bad_gfx.state);
    TEST_ASSERT_TRUE(rts_renderer_init(&rr, rts_gfx_null_create(), &vfs, 1280, 720));

    /* Exercise all showcase scenes + live-world floating-origin render + screenshot capture */
    for (int s = 0; s < RTS_SCENE_COUNT; ++s) {
        rts_renderer_render_scene(&rr, &world, (s == RTS_SCENE_MOBILE_TOUCH_HUD ? NULL : &input), (RtsShowcaseScene)s, 1.5f + (float)s);
    }
    const char *cls_ids[6] = {"bldg", "veh", "ship", "air", "space", "stn"};
    for (int c = 0; c < 6; ++c) {
        RtsConstructBlueprint bp;
        memset(&bp, 0, sizeof(bp));
        snprintf(bp.id, sizeof(bp.id), "%s", cls_ids[c]);
        bp.construct_class = (RtsConstructClass)c;
        bp.max_hull = 100.0f;
        rts_world_register_blueprint(&world, &bp);
        rts_world_spawn_from_blueprint(&world, cls_ids[c], cls_ids[c], (uint8_t)(c % 2 + 1), world.planet_frame_id, rts_v3d((double)c * 20.0, 0.0, -40.0), false);
    }
    rts_renderer_render_live_world(&rr, &world, &input, 2.0f);
    rts_renderer_render_webxr_stereo(&rr, &world, &input, NULL, 2.5f);
    TEST_ASSERT_TRUE(rts_renderer_capture_png(&rr, "/tmp/rts_null_shot.png"));
    TEST_ASSERT_FALSE(rts_renderer_capture_png(&rr, NULL));

    RtsGfxStats st = rr.gfx.get_stats(rr.gfx.state);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)RTS_SCENE_COUNT + 2u, st.frames_begun);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)RTS_SCENE_COUNT + 2u, st.frames_ended);
    TEST_ASSERT_TRUE(st.draw_calls > 150u);
    TEST_ASSERT_EQUAL_UINT32(1u, st.screenshots_saved);

    rr.gfx.destroy_mesh(rr.gfx.state, rr.mesh_cube);
    rts_renderer_shutdown(&rr);
}

#include "rts/lua_vm.h"
#include <stdlib.h>

static void test_all_76_models_gallery_and_live_interiors(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    RtsWorld *world = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    TEST_ASSERT_NOT_NULL(world);
    rts_world_init(world, rts_planet_earthlike(42u), 999u);
    RtsInputRouter input;
    rts_input_init(&input, 1600.0f, 960.0f, false);
    RtsVoxelChunk voxel;
    rts_voxel_init(&voxel, 1.0f);
    RtsRetroRenderer rr;
    TEST_ASSERT_TRUE(rts_renderer_init(&rr, rts_gfx_null_create(), &vfs, 1600, 960));

    RtsLuaVm vm;
    TEST_ASSERT_TRUE(rts_lua_init(&vm, &vfs, world, &input, &voxel, &rr));
    TEST_ASSERT_TRUE_MESSAGE(rts_lua_exec_vfs_script(&vm, "main.lua"), vm.last_error);
    TEST_ASSERT_EQUAL_UINT(76u, world->blueprint_count);

    /* Verify all 76 unique sculpted .obj models load & cache cleanly via VFS */
    for (size_t i = 0; i < world->blueprint_count; ++i) {
        const RtsConstructBlueprint *bp = &world->blueprints[i];
        RtsGfxHandle h1 = rts_renderer_get_mesh(&rr, bp->mesh_path);
        RtsGfxHandle h2 = rts_renderer_get_mesh(&rr, bp->mesh_path);
        TEST_ASSERT_NOT_EQUAL_UINT32(RTS_GFX_INVALID, h1);
        TEST_ASSERT_EQUAL_UINT32(h1, h2);
    }
    TEST_ASSERT_EQUAL_UINT(76u, rr.mesh_cache_count);
    TEST_ASSERT_EQUAL_UINT32(RTS_GFX_INVALID, rts_renderer_get_mesh(&rr, "models/does_not_exist.obj"));
    TEST_ASSERT_EQUAL_UINT32(RTS_GFX_INVALID, rts_renderer_get_mesh(NULL, "models/citadel.obj"));

    /* Render all 6 class galleries covering all 76 constructs */
    for (int c = 0; c < 6; ++c) {
        rts_renderer_render_roster_class(&rr, world, &input, (RtsConstructClass)c, 0.5f + (float)c);
    }

    /* Render live world inside Citadel War Room (Holo-Globe active + Console prompt + Surface EVA + 400km LEO Orbit) */
    rts_renderer_set_live_status(&rr, "SANDBOX TEST", "", "TOAST OK");
    world->commander.at_holo_globe = true;
    rts_renderer_render_live_world(&rr, world, &input, 1.0f);

    world->commander.at_holo_globe = false;
    world->commander.local_pos = rts_v3d(0.0, 1.1, -3.5);
    rts_renderer_render_live_world(&rr, world, &input, 1.5f);

    rts_renderer_set_live_status(&rr, "SANDBOX TEST", "CUSTOM PROMPT", "TOAST 2");
    rts_renderer_render_live_world(&rr, world, &input, 1.8f);

    /* Move commander into 400km Orbital Shipyard to verify orbital altitude sky & Helios Prime limb */
    for (size_t i = 0; i < world->entity_count; ++i) {
        if (strcmp(world->entities[i].blueprint_id, "stn_orbital_shipyard") == 0) {
            world->commander.frame_id = world->entities[i].own_frame_id;
            world->commander.occupied_entity_id = world->entities[i].id;
            world->commander.local_pos = rts_v3d(0.0, 1.7, 0.0);
            break;
        }
    }
    rts_renderer_render_live_world(&rr, world, &input, 2.2f);

    /* Move commander outside onto planetary surface */
    world->commander.frame_id = world->planet_frame_id;
    world->commander.occupied_entity_id = 0u;
    world->commander.local_pos = rts_v3d(20.0, world->planet.radius_m + 1.7, 20.0);
    rts_renderer_render_live_world(&rr, world, &input, 2.6f);

    RtsGfxStats st = rr.gfx.get_stats(rr.gfx.state);
    TEST_ASSERT_EQUAL_UINT32(11u, st.frames_begun);
    TEST_ASSERT_EQUAL_UINT32(11u, st.frames_ended);
    TEST_ASSERT_TRUE(st.draw_calls > 300u);

    rts_lua_shutdown(&vm);
    rts_renderer_shutdown(&rr);
    free(world);
}

void register_assets_and_renderer_tests(void) {
    RUN_TEST(test_obj_and_png_io);
    RUN_TEST(test_retro_renderer_all_scenes_on_null_backend);
    RUN_TEST(test_all_76_models_gallery_and_live_interiors);
}
