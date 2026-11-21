/* SPDX-License-Identifier: MIT */
#ifndef RTS_INPUT_H
#define RTS_INPUT_H

#include "rts/math3d.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RtsInputMode {
    RTS_INPUT_DESKTOP_KBM = 0,
    RTS_INPUT_GAMEPAD = 1,
    RTS_INPUT_MOBILE_TOUCH = 2,
    RTS_INPUT_WEBXR = 3
} RtsInputMode;

typedef enum RtsTouchButtonId {
    RTS_TBTN_INTERACT = 0,
    RTS_TBTN_WRIST_PAD = 1,
    RTS_TBTN_DESIGNATE = 2,
    RTS_TBTN_CMD_WHEEL = 3,
    RTS_TBTN_MAG_BOOT = 4,
    RTS_TBTN_COUNT = 5
} RtsTouchButtonId;

typedef struct RtsRectf {
    float x, y, w, h;
} RtsRectf;

typedef struct RtsTouchLayout {
    float screen_w;
    float screen_h;
    float stick_cx;
    float stick_cy;
    float stick_radius;
    RtsRectf buttons[RTS_TBTN_COUNT];
    const char *labels[RTS_TBTN_COUNT];
} RtsTouchLayout;

typedef struct RtsXrEyeView {
    RtsMat4f view;
    RtsMat4f proj;
    int viewport[4];
} RtsXrEyeView;

typedef struct RtsXrFrameState {
    bool session_active;
    int eye_count;
    RtsXrEyeView eyes[2];
    RtsVec3d head_pos;
    RtsQuatd head_rot;
    RtsRay right_aim_ray;
    bool right_trigger_down;
    bool right_trigger_pressed;
    float left_stick_x;
    float left_stick_y;
    float right_stick_x;
} RtsXrFrameState;

typedef struct RtsInputIntent {
    float move_x;
    float move_y;
    float look_dx;
    float look_dy;
    bool interact_pressed;
    bool toggle_pad_pressed;
    bool designate_held;
    bool command_wheel_held;
    bool mag_boot_pressed;
    bool holo_select_pressed;
    bool holo_order_pressed;
    float holo_zoom_delta;
    RtsRay aim_ray;
    bool has_aim_ray;
} RtsInputIntent;

typedef struct RtsInputRouter {
    RtsInputMode mode;
    bool mode_override_locked;
    bool coarse_pointer_detected;
    RtsTouchLayout touch_layout;

    /* Active touch tracking */
    bool stick_active;
    int64_t stick_finger_id;
    float stick_knob_x;
    float stick_knob_y;

    bool look_active;
    int64_t look_finger_id;
    float look_last_x;
    float look_last_y;

    bool pinch_active;
    float pinch_last_dist;

    RtsXrFrameState xr;
    RtsInputIntent intent;
} RtsInputRouter;

void rts_input_init(RtsInputRouter *r, float screen_w, float screen_h, bool coarse_pointer);
void rts_input_resize(RtsInputRouter *r, float screen_w, float screen_h);
void rts_input_begin_frame(RtsInputRouter *r);
void rts_input_set_mode_override(RtsInputRouter *r, bool locked, RtsInputMode mode);
void rts_input_notify_kbm_activity(RtsInputRouter *r);
void rts_input_notify_gamepad_activity(RtsInputRouter *r);
void rts_input_on_touch_down(RtsInputRouter *r, int64_t finger_id, float nx, float ny);
void rts_input_on_touch_move(RtsInputRouter *r, int64_t finger_id, float nx, float ny);
void rts_input_on_touch_up(RtsInputRouter *r, int64_t finger_id);
void rts_input_on_pinch(RtsInputRouter *r, float dist_norm, bool active);
void rts_input_update_xr(RtsInputRouter *r, const RtsXrFrameState *xr);

#endif /* RTS_INPUT_H */
