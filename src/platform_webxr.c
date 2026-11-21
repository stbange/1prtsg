/* SPDX-License-Identifier: MIT */
#include "rts/platform.h"
#include <string.h>

static int g_web_touch_device = 0;
static RtsXrFrameState g_xr_state;

void rts_web_notify_touch_device(int is_coarse_touch) {
    g_web_touch_device = is_coarse_touch ? 1 : 0;
}

int rts_web_is_touch_device(void) {
    return g_web_touch_device;
}

void rts_webxr_on_session_state(int active) {
    g_xr_state.session_active = (active != 0);
}

void rts_webxr_on_frame(const RtsXrFrameState *state) {
    if (state) {
        g_xr_state = *state;
    }
}

bool rts_webxr_get_latest_state(RtsXrFrameState *out_state) {
    if (!out_state) return false;
    *out_state = g_xr_state;
    return g_xr_state.session_active;
}
