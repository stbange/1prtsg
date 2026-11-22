/* SPDX-License-Identifier: MIT */
#include "rts/gfx.h"
#include "rts/input.h"
#include "rts/platform.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void test_webxr_6dof_submit_pose_snap_turn_and_stereo_renderer(void) {
    /* Build realistic 6-DOF left & right eye matrices */
    RtsMat4f proj = rts_mat4_perspective((float)(72.0 * RTS_DEG2RAD), 0.9f, 0.1f, 1200.0f);
    RtsMat4f lview = rts_mat4_look_at((RtsVec3f){-0.032f, 2.25f, 5.5f}, (RtsVec3f){0.0f, 1.9f, -0.6f}, (RtsVec3f){0.0f, 1.0f, 0.0f});
    RtsMat4f rview = rts_mat4_look_at((RtsVec3f){ 0.032f, 2.25f, 5.5f}, (RtsVec3f){0.0f, 1.9f, -0.6f}, (RtsVec3f){0.0f, 1.0f, 0.0f});

    rts_webxr_on_session_state(1);
    rts_webxr_submit_pose(0.0f, 1.70f, 5.5f, lview.m, proj.m, rview.m, proj.m,
                          -0.62f, 1.42f, 4.30f, 0.65f, 1.48f, 4.10f, -0.15f, 0.16f, -0.97f,
                          1, 0.4f, 0.8f, 0.85f, 1, 1);

    RtsXrFrameState xr;
    TEST_ASSERT_TRUE(rts_webxr_get_latest_state(&xr));
    TEST_ASSERT_TRUE(xr.session_active);
    TEST_ASSERT_TRUE(xr.has_eye_matrices);
    TEST_ASSERT_TRUE(xr.comfort_vignette);
    TEST_ASSERT_TRUE(xr.seated_cockpit_mode);
    TEST_ASSERT_TRUE(xr.right_trigger_pressed);
    TEST_ASSERT_DOUBLE_WITHIN(0.01, 2.25, xr.head_pos.y);

    /* Snap-turn latching in RtsInputRouter */
    RtsInputRouter inp;
    rts_input_init(&inp, 1280.0f, 720.0f, false);
    rts_input_begin_frame(&inp);
    rts_input_update_xr(&inp, &xr);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_WEBXR, inp.mode);
    TEST_ASSERT_TRUE(fabsf(inp.intent.look_dx) > 0.4f); /* ~30 deg snap turn */

    /* Holding stick right on next frame must NOT re-trigger snap turn until centered */
    rts_input_begin_frame(&inp);
    rts_input_update_xr(&inp, &xr);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, inp.intent.look_dx);

    /* Center stick to reset latch, then verify negative snap turn */
    xr.right_stick_x = 0.0f;
    rts_input_begin_frame(&inp);
    rts_input_update_xr(&inp, &xr);
    xr.right_stick_x = -0.9f;
    rts_input_begin_frame(&inp);
    rts_input_update_xr(&inp, &xr);
    TEST_ASSERT_TRUE(inp.intent.look_dx < -0.4f);

    /* Render true 6-DOF eye matrices + comfort vignette + left-hand Wrist-Pad on null backend */
    RtsVfs vfs;
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(rr);
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    rts_world_init(w, rts_planet_earthlike(42u), 11u);
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1600, 900));
    rts_renderer_render_webxr_stereo(rr, w, &inp, &xr, 1.5f);
    TEST_ASSERT_TRUE(rr->gfx.get_stats(rr->gfx.state).draw_calls > 30u);

    /* Clean session teardown */
    rts_webxr_on_session_state(0);
    TEST_ASSERT_FALSE(rts_webxr_get_latest_state(&xr));
    rts_renderer_shutdown(rr);
    free(rr);
    free(w);
}

static void test_mobile_2finger_pinch_rotate_hologlobe_gesture(void) {
    RtsInputRouter inp;
    rts_input_init(&inp, 1080.0f, 1920.0f, true);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_MOBILE_TOUCH, inp.mode);

    rts_web_on_pinch_gesture(0.30f, 0.10f, 1);
    float d = 0.0f, a = 0.0f;
    bool act = false;
    TEST_ASSERT_TRUE(rts_web_consume_pinch(&d, &a, &act));
    TEST_ASSERT_TRUE(act);
    rts_input_begin_frame(&inp);
    rts_input_on_pinch_rotate(&inp, d, a, act);

    rts_web_on_pinch_gesture(0.55f, 0.45f, 1);
    TEST_ASSERT_TRUE(rts_web_consume_pinch(&d, &a, &act));
    rts_input_on_pinch_rotate(&inp, d, a, act);
    TEST_ASSERT_TRUE(inp.intent.holo_zoom_delta > 0.5f);
    TEST_ASSERT_TRUE(inp.intent.holo_orbit_dx > 0.5f);

    rts_web_on_pinch_gesture(0.0f, 0.0f, 0);
    TEST_ASSERT_TRUE(rts_web_consume_pinch(&d, &a, &act));
    TEST_ASSERT_FALSE(act);
    rts_input_on_pinch_rotate(&inp, d, a, act);
    TEST_ASSERT_FALSE(inp.pinch_active);
    /* Cover touch device hook, on_frame, null eye matrices, and null out_state */
    rts_web_notify_touch_device(1);
    TEST_ASSERT_EQUAL_INT(1, rts_web_is_touch_device());
    rts_web_notify_touch_device(0);
    TEST_ASSERT_EQUAL_INT(0, rts_web_is_touch_device());

    RtsXrFrameState custom_xr;
    memset(&custom_xr, 0, sizeof(custom_xr));
    custom_xr.session_active = true;
    rts_webxr_on_frame(&custom_xr);
    rts_webxr_on_frame(NULL);
    TEST_ASSERT_FALSE(rts_webxr_get_latest_state(NULL));

    rts_webxr_submit_pose(0.0f, 1.7f, 2.0f, NULL, NULL, NULL, NULL,
                          -0.5f, 1.3f, 1.5f, 0.5f, 1.3f, 1.5f, 0.0f, 0.0f, -1.0f,
                          0, 0.0f, 0.0f, 0.0f, 0, 0);
    TEST_ASSERT_TRUE(rts_webxr_get_latest_state(&custom_xr));
    TEST_ASSERT_FALSE(custom_xr.has_eye_matrices);
    rts_webxr_on_session_state(0);
}

void register_webxr_mobile_tests(void) {
    RUN_TEST(test_webxr_6dof_submit_pose_snap_turn_and_stereo_renderer);
    RUN_TEST(test_mobile_2finger_pinch_rotate_hologlobe_gesture);
}
