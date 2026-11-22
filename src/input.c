/* SPDX-License-Identifier: MIT */
#include "rts/input.h"
#include <math.h>
#include <string.h>

static void compute_touch_layout(RtsTouchLayout *l, float w, float h) {
    l->screen_w = w > 1.0f ? w : 1280.0f;
    l->screen_h = h > 1.0f ? h : 720.0f;
    float min_dim = l->screen_w < l->screen_h ? l->screen_w : l->screen_h;
    l->stick_radius = min_dim * 0.14f;
    l->stick_cx = l->stick_radius * 1.45f;
    l->stick_cy = l->screen_h - l->stick_radius * 1.35f;

    float bw = min_dim * 0.15f;
    float bh = min_dim * 0.085f;
    float gap = min_dim * 0.025f;
    float rx = l->screen_w - bw - gap * 1.4f;
    float base_y = l->screen_h - bh - gap * 1.6f;

    static const char *k_labels[RTS_TBTN_COUNT] = {
        "ENTER", "PAD", "LASER", "ORDERS", "MAG"
    };
    for (int i = 0; i < RTS_TBTN_COUNT; ++i) {
        l->labels[i] = k_labels[i];
    }

    /* Ergonomic right-side tactical cluster */
    l->buttons[RTS_TBTN_INTERACT]  = (RtsRectf){rx, base_y, bw, bh};
    l->buttons[RTS_TBTN_CMD_WHEEL] = (RtsRectf){rx - bw - gap, base_y, bw, bh};
    l->buttons[RTS_TBTN_WRIST_PAD] = (RtsRectf){rx, base_y - (bh + gap), bw, bh};
    l->buttons[RTS_TBTN_DESIGNATE] = (RtsRectf){rx - bw - gap, base_y - (bh + gap), bw, bh};
    l->buttons[RTS_TBTN_MAG_BOOT]  = (RtsRectf){rx, base_y - 2.0f * (bh + gap), bw, bh};
}

static bool rect_contains(RtsRectf rc, float px, float py) {
    return px >= rc.x && px <= (rc.x + rc.w) && py >= rc.y && py <= (rc.y + rc.h);
}

void rts_input_init(RtsInputRouter *r, float screen_w, float screen_h, bool coarse_pointer) {
    memset(r, 0, sizeof(*r));
    r->coarse_pointer_detected = coarse_pointer;
    r->mode = coarse_pointer ? RTS_INPUT_MOBILE_TOUCH : RTS_INPUT_DESKTOP_KBM;
    compute_touch_layout(&r->touch_layout, screen_w, screen_h);
    r->stick_knob_x = r->touch_layout.stick_cx;
    r->stick_knob_y = r->touch_layout.stick_cy;
}

void rts_input_resize(RtsInputRouter *r, float screen_w, float screen_h) {
    if (!r) return;
    compute_touch_layout(&r->touch_layout, screen_w, screen_h);
    if (!r->stick_active) {
        r->stick_knob_x = r->touch_layout.stick_cx;
        r->stick_knob_y = r->touch_layout.stick_cy;
    }
}

void rts_input_begin_frame(RtsInputRouter *r) {
    if (!r) return;
    r->intent.look_dx = 0.0f;
    r->intent.look_dy = 0.0f;
    r->intent.interact_pressed = false;
    r->intent.toggle_pad_pressed = false;
    r->intent.mag_boot_pressed = false;
    r->intent.holo_select_pressed = false;
    r->intent.holo_order_pressed = false;
    r->intent.holo_zoom_delta = 0.0f;
    if (!r->stick_active && r->mode == RTS_INPUT_MOBILE_TOUCH) {
        r->intent.move_x = 0.0f;
        r->intent.move_y = 0.0f;
    }
}

void rts_input_set_mode_override(RtsInputRouter *r, bool locked, RtsInputMode mode) {
    if (!r) return;
    r->mode_override_locked = locked;
    r->mode = mode;
}

void rts_input_notify_kbm_activity(RtsInputRouter *r) {
    if (!r || r->mode_override_locked || r->xr.session_active) return;
    r->mode = RTS_INPUT_DESKTOP_KBM;
}

void rts_input_notify_gamepad_activity(RtsInputRouter *r) {
    if (!r || r->mode_override_locked || r->xr.session_active) return;
    r->mode = RTS_INPUT_GAMEPAD;
}

static void update_stick_from_px(RtsInputRouter *r, float px, float py) {
    float dx = px - r->touch_layout.stick_cx;
    float dy = py - r->touch_layout.stick_cy;
    float dist = sqrtf(dx * dx + dy * dy);
    float rad = r->touch_layout.stick_radius;
    if (dist > rad && dist > 1e-4f) {
        dx = (dx / dist) * rad;
        dy = (dy / dist) * rad;
    }
    r->stick_knob_x = r->touch_layout.stick_cx + dx;
    r->stick_knob_y = r->touch_layout.stick_cy + dy;
    r->intent.move_x = rts_clampf(dx / rad, -1.0f, 1.0f);
    r->intent.move_y = rts_clampf(-dy / rad, -1.0f, 1.0f); /* Screen Y down -> forward +Y */
}

void rts_input_on_touch_down(RtsInputRouter *r, int64_t finger_id, float nx, float ny) {
    if (!r) return;
    if (!r->mode_override_locked && !r->xr.session_active) {
        r->mode = RTS_INPUT_MOBILE_TOUCH;
    }
    float px = nx * r->touch_layout.screen_w;
    float py = ny * r->touch_layout.screen_h;

    for (int i = 0; i < RTS_TBTN_COUNT; ++i) {
        if (rect_contains(r->touch_layout.buttons[i], px, py)) {
            if (i == RTS_TBTN_INTERACT)  r->intent.interact_pressed = true;
            if (i == RTS_TBTN_WRIST_PAD) r->intent.toggle_pad_pressed = true;
            if (i == RTS_TBTN_DESIGNATE) r->intent.designate_held = true;
            if (i == RTS_TBTN_CMD_WHEEL) r->intent.command_wheel_held = true;
            if (i == RTS_TBTN_MAG_BOOT)  r->intent.mag_boot_pressed = true;
            return;
        }
    }

    if (nx < 0.45f && !r->stick_active) {
        r->stick_active = true;
        r->stick_finger_id = finger_id;
        update_stick_from_px(r, px, py);
        return;
    }

    if (!r->look_active) {
        r->look_active = true;
        r->look_finger_id = finger_id;
        r->look_last_x = px;
        r->look_last_y = py;
        r->intent.holo_select_pressed = true;
    }
}

void rts_input_on_touch_move(RtsInputRouter *r, int64_t finger_id, float nx, float ny) {
    if (!r) return;
    float px = nx * r->touch_layout.screen_w;
    float py = ny * r->touch_layout.screen_h;

    if (r->stick_active && r->stick_finger_id == finger_id) {
        update_stick_from_px(r, px, py);
    } else if (r->look_active && r->look_finger_id == finger_id) {
        r->intent.look_dx += (px - r->look_last_x) * 0.005f;
        r->intent.look_dy += (py - r->look_last_y) * 0.005f;
        r->look_last_x = px;
        r->look_last_y = py;
    }
}

void rts_input_on_touch_up(RtsInputRouter *r, int64_t finger_id) {
    if (!r) return;
    if (r->stick_active && r->stick_finger_id == finger_id) {
        r->stick_active = false;
        r->stick_knob_x = r->touch_layout.stick_cx;
        r->stick_knob_y = r->touch_layout.stick_cy;
        r->intent.move_x = 0.0f;
        r->intent.move_y = 0.0f;
    }
    if (r->look_active && r->look_finger_id == finger_id) {
        r->look_active = false;
    }
    r->intent.designate_held = false;
    r->intent.command_wheel_held = false;
}

void rts_input_on_pinch(RtsInputRouter *r, float dist_norm, bool active) {
    rts_input_on_pinch_rotate(r, dist_norm, 0.0f, active);
}

void rts_input_on_pinch_rotate(RtsInputRouter *r, float dist_norm, float angle_rad, bool active) {
    if (!r) return;
    if (!active) {
        r->pinch_active = false;
        return;
    }
    if (!r->pinch_active) {
        r->pinch_active = true;
        r->pinch_last_dist = dist_norm;
        r->pinch_last_angle = angle_rad;
        return;
    }
    r->intent.holo_zoom_delta += (dist_norm - r->pinch_last_dist) * 4.0f;
    r->intent.holo_orbit_dx += (angle_rad - r->pinch_last_angle) * 2.5f;
    r->pinch_last_dist = dist_norm;
    r->pinch_last_angle = angle_rad;
}

void rts_input_update_xr(RtsInputRouter *r, const RtsXrFrameState *xr) {
    if (!r || !xr) return;
    r->xr = *xr;
    if (xr->session_active) {
        r->mode = RTS_INPUT_WEBXR;
        r->intent.move_x = rts_clampf(xr->left_stick_x, -1.0f, 1.0f);
        r->intent.move_y = rts_clampf(xr->left_stick_y, -1.0f, 1.0f);
        if (xr->snap_turn_deg > 1.0f) {
            if (fabsf(xr->right_stick_x) > 0.65f) {
                if (!r->xr_snap_latched) {
                    float dir = (xr->right_stick_x > 0.0f) ? 1.0f : -1.0f;
                    r->intent.look_dx = dir * (float)((double)xr->snap_turn_deg * RTS_DEG2RAD);
                    r->xr_snap_latched = true;
                }
            } else {
                r->xr_snap_latched = false;
            }
        } else {
            r->intent.look_dx = xr->right_stick_x * 0.05f;
        }
        r->intent.aim_ray = xr->right_aim_ray;
        r->intent.has_aim_ray = true;
        r->intent.holo_select_pressed = xr->right_trigger_pressed;
        r->intent.interact_pressed = xr->right_trigger_pressed;
    } else if (r->mode == RTS_INPUT_WEBXR) {
        r->mode = r->coarse_pointer_detected ? RTS_INPUT_MOBILE_TOUCH : RTS_INPUT_DESKTOP_KBM;
        r->intent.has_aim_ray = false;
    }
}
