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
    const char *fun_frames_dir;
    const char *roster_gallery_dir;
    const char *mega_interiors_dir;
    const char *rts_toolkit_dir;
    const char *milestone_011_dir;
    const char *milestone_012_dir;
    const char *milestone_013_dir;
    const char *telemetry_out;
    const char *mod_to_mount;
    int width;
    int height;
    bool headless;
    bool lua_coverage;
    bool fun_eval;
    bool fun_baseline;
    uint64_t seed;
    int fun_episodes;
    double min_lua_coverage;
} RtsAppConfig;

int rts_platform_run_interactive(const RtsAppConfig *cfg);
int rts_platform_capture_screenshots(const RtsAppConfig *cfg);
int rts_platform_capture_fun_frames(const RtsAppConfig *cfg);
int rts_platform_capture_roster_gallery(const RtsAppConfig *cfg);
int rts_platform_capture_mega_interiors(const RtsAppConfig *cfg);
int rts_platform_capture_rts_toolkit(const RtsAppConfig *cfg);
int rts_platform_capture_milestone_011(const RtsAppConfig *cfg);
int rts_platform_capture_milestone_012(const RtsAppConfig *cfg);
int rts_platform_capture_milestone_013(const RtsAppConfig *cfg);

/* Emscripten / WebXR 6-DOF & Mobile Touch bridge entry points */
void rts_web_notify_touch_device(int is_coarse_touch);
int rts_web_is_touch_device(void);
void rts_web_on_pinch_gesture(float dist_norm, float angle_rad, int active);
bool rts_web_consume_pinch(float *out_dist, float *out_angle, bool *out_active);
void rts_webxr_on_session_state(int active);
void rts_webxr_on_frame(const RtsXrFrameState *state);
void rts_webxr_submit_pose(float head_x, float head_y, float head_z,
                           const float *left_view16, const float *left_proj16,
                           const float *right_view16, const float *right_proj16,
                           float lhand_x, float lhand_y, float lhand_z,
                           float rhand_ox, float rhand_oy, float rhand_oz,
                           float rhand_dx, float rhand_dy, float rhand_dz,
                           int trigger_down, float lstick_x, float lstick_y, float rstick_x,
                           int comfort_vignette, int seated_mode);
bool rts_webxr_get_latest_state(RtsXrFrameState *out_state);

#endif /* RTS_PLATFORM_H */
