/* SPDX-License-Identifier: MIT */
#include "rts/input.h"
#include "unity.h"
#include <string.h>

static void test_input_auto_touch_buttons_and_webxr(void) {
    RtsInputRouter r;
    rts_input_init(&r, 1280.0f, 720.0f, false);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_DESKTOP_KBM, r.mode);

    rts_input_notify_gamepad_activity(&r);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_GAMEPAD, r.mode);
    rts_input_notify_kbm_activity(&r);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_DESKTOP_KBM, r.mode);

    rts_input_resize(&r, 1000.0f, 600.0f);

    /* Touch on left thumbstick auto-switches to MOBILE_TOUCH and drives move axes */
    rts_input_begin_frame(&r);
    rts_input_on_touch_down(&r, 10, 0.12f, 0.82f);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_MOBILE_TOUCH, r.mode);
    rts_input_on_touch_move(&r, 10, 0.35f, 0.60f); /* clamps to stick radius */
    TEST_ASSERT_TRUE(r.intent.move_x > 0.3f && r.intent.move_y > 0.3f);

    /* Touch look drag on right side + pinch zoom */
    rts_input_on_touch_down(&r, 11, 0.60f, 0.40f);
    TEST_ASSERT_TRUE(r.intent.holo_select_pressed);
    rts_input_on_touch_move(&r, 11, 0.68f, 0.45f);
    TEST_ASSERT_TRUE(r.intent.look_dx > 0.0f && r.intent.look_dy > 0.0f);

    rts_input_on_pinch(&r, 0.20f, true);
    rts_input_on_pinch(&r, 0.35f, true);
    TEST_ASSERT_TRUE(r.intent.holo_zoom_delta > 0.1f);
    rts_input_on_pinch(&r, 0.35f, false);

    rts_input_on_touch_up(&r, 10);
    rts_input_on_touch_up(&r, 11);
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.0f, r.intent.move_x);

    /* Press all 5 tactical touch buttons via their computed rects */
    for (int i = 0; i < RTS_TBTN_COUNT; ++i) {
        rts_input_begin_frame(&r);
        RtsRectf rc = r.touch_layout.buttons[i];
        float nx = (rc.x + rc.w * 0.5f) / r.touch_layout.screen_w;
        float ny = (rc.y + rc.h * 0.5f) / r.touch_layout.screen_h;
        rts_input_on_touch_down(&r, 20 + i, nx, ny);
        if (i == RTS_TBTN_INTERACT)  TEST_ASSERT_TRUE(r.intent.interact_pressed);
        if (i == RTS_TBTN_WRIST_PAD) TEST_ASSERT_TRUE(r.intent.toggle_pad_pressed);
        if (i == RTS_TBTN_DESIGNATE) TEST_ASSERT_TRUE(r.intent.designate_held);
        if (i == RTS_TBTN_CMD_WHEEL) TEST_ASSERT_TRUE(r.intent.command_wheel_held);
        if (i == RTS_TBTN_MAG_BOOT)  TEST_ASSERT_TRUE(r.intent.mag_boot_pressed);
        rts_input_on_touch_up(&r, 20 + i);
    }

    /* Mode override lock */
    rts_input_set_mode_override(&r, true, RTS_INPUT_DESKTOP_KBM);
    rts_input_notify_gamepad_activity(&r);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_DESKTOP_KBM, r.mode);

    /* WebXR session activation & clean exit */
    RtsXrFrameState xr;
    memset(&xr, 0, sizeof(xr));
    xr.session_active = true;
    xr.left_stick_x = 0.5f;
    xr.left_stick_y = 0.8f;
    xr.right_stick_x = -0.4f;
    xr.right_trigger_pressed = true;
    rts_input_update_xr(&r, &xr);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_WEBXR, r.mode);
    TEST_ASSERT_TRUE(r.intent.has_aim_ray && r.intent.holo_select_pressed);
    xr.session_active = false;
    rts_input_update_xr(&r, &xr);
    TEST_ASSERT_EQUAL_INT(RTS_INPUT_DESKTOP_KBM, r.mode);
}

void register_input_tests(void) {
    RUN_TEST(test_input_auto_touch_buttons_and_webxr);
}
