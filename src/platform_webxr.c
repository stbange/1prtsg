/* SPDX-License-Identifier: MIT */
#include "rts/platform.h"
#include <string.h>

static int g_web_touch_device = 0;
static RtsXrFrameState g_xr_state;
static bool g_pinch_dirty = false;
static float g_pinch_dist = 0.0f;
static float g_pinch_angle = 0.0f;
static bool g_pinch_active = false;

void rts_web_notify_touch_device(int is_coarse_touch) {
    g_web_touch_device = is_coarse_touch ? 1 : 0;
}

int rts_web_is_touch_device(void) {
    return g_web_touch_device;
}

void rts_web_on_pinch_gesture(float dist_norm, float angle_rad, int active) {
    g_pinch_dist = dist_norm;
    g_pinch_angle = angle_rad;
    g_pinch_active = (active != 0);
    g_pinch_dirty = true;
}

bool rts_web_consume_pinch(float *out_dist, float *out_angle, bool *out_active) {
    if (!g_pinch_dirty) return false;
    g_pinch_dirty = false;
    if (out_dist) *out_dist = g_pinch_dist;
    if (out_angle) *out_angle = g_pinch_angle;
    if (out_active) *out_active = g_pinch_active;
    return true;
}

void rts_webxr_on_session_state(int active) {
    g_xr_state.session_active = (active != 0);
    if (!g_xr_state.session_active) {
        g_xr_state.has_eye_matrices = false;
        g_xr_state.right_trigger_down = false;
        g_xr_state.right_trigger_pressed = false;
    }
}

void rts_webxr_on_frame(const RtsXrFrameState *state) {
    if (state) {
        g_xr_state = *state;
    }
}

void rts_webxr_submit_pose(float head_x, float head_y, float head_z,
                           const float *left_view16, const float *left_proj16,
                           const float *right_view16, const float *right_proj16,
                           float lhand_x, float lhand_y, float lhand_z,
                           float rhand_ox, float rhand_oy, float rhand_oz,
                           float rhand_dx, float rhand_dy, float rhand_dz,
                           int trigger_down, float lstick_x, float lstick_y, float rstick_x,
                           int comfort_vignette, int seated_mode) {
    bool prev_trig = g_xr_state.right_trigger_down;
    g_xr_state.session_active = true;
    g_xr_state.eye_count = 2;
    g_xr_state.head_pos = rts_v3d((double)head_x, (double)head_y + (seated_mode ? 0.55 : 0.0), (double)head_z);
    g_xr_state.left_hand_pos = rts_v3d((double)lhand_x, (double)lhand_y, (double)lhand_z);
    g_xr_state.left_hand_rot = rts_quat_identity();
    g_xr_state.right_aim_ray.origin = rts_v3d((double)rhand_ox, (double)rhand_oy, (double)rhand_oz);
    g_xr_state.right_aim_ray.dir = rts_v3d_norm(rts_v3d((double)rhand_dx, (double)rhand_dy, (double)rhand_dz));
    g_xr_state.right_trigger_down = (trigger_down != 0);
    g_xr_state.right_trigger_pressed = (g_xr_state.right_trigger_down && !prev_trig);
    g_xr_state.left_stick_x = lstick_x;
    g_xr_state.left_stick_y = lstick_y;
    g_xr_state.right_stick_x = rstick_x;
    g_xr_state.snap_turn_deg = 30.0f;
    g_xr_state.comfort_vignette = (comfort_vignette != 0);
    g_xr_state.seated_cockpit_mode = (seated_mode != 0);

    if (left_view16 && left_proj16 && right_view16 && right_proj16) {
        memcpy(g_xr_state.eyes[0].view.m, left_view16, 16u * sizeof(float));
        memcpy(g_xr_state.eyes[0].proj.m, left_proj16, 16u * sizeof(float));
        memcpy(g_xr_state.eyes[1].view.m, right_view16, 16u * sizeof(float));
        memcpy(g_xr_state.eyes[1].proj.m, right_proj16, 16u * sizeof(float));
        g_xr_state.has_eye_matrices = true;
    } else {
        g_xr_state.has_eye_matrices = false;
    }
}

bool rts_webxr_get_latest_state(RtsXrFrameState *out_state) {
    if (!out_state) return false;
    *out_state = g_xr_state;
    return g_xr_state.session_active;
}
