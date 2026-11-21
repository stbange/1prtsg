/* SPDX-License-Identifier: MIT */
#ifndef RTS_PLATFORM_H
#define RTS_PLATFORM_H

#include "rts/gfx.h"
#include "rts/input.h"
#include <stdbool.h>

typedef struct RtsAppConfig {
    const char *data_dir;
    const char *test_script;
    const char *screenshot_dir;
    const char *mod_to_mount;
    int width;
    int height;
    bool headless;
    bool lua_coverage;
    double min_lua_coverage;
} RtsAppConfig;

int rts_platform_run_interactive(const RtsAppConfig *cfg);
int rts_platform_capture_screenshots(const RtsAppConfig *cfg);

/* Emscripten / WebXR bridge entry points */
void rts_web_notify_touch_device(int is_coarse_touch);
void rts_webxr_on_session_state(int active);
void rts_webxr_on_frame(const RtsXrFrameState *state);
bool rts_webxr_get_latest_state(RtsXrFrameState *out_state);

#endif /* RTS_PLATFORM_H */
