# Design: 6-DOF WebXR Frame Streaming & Mobile Gesture Ergonomics

- **C ABI Bridge (`include/rts/platform.h`, `src/platform_webxr.c`)**:
  - `void rts_webxr_submit_pose(float head_x, float head_y, float head_z, const float *left_view16, const float *left_proj16, const float *right_view16, const float *right_proj16, float lhand_x, float lhand_y, float lhand_z, float rhand_ox, float rhand_oy, float rhand_oz, float rhand_dx, float rhand_dy, float rhand_dz, int trigger_down, float lstick_x, float lstick_y, float rstick_x, int comfort_vignette, int seated_mode);`
  - Exported in Emscripten `EXPORTED_FUNCTIONS` so `web/xr_bridge.js` streams live `XRViewerPose` + `XRInputSource` data directly on `xrSession.requestAnimationFrame`.
- **2-Finger Pinch & Rotate (`include/rts/input.h`, `src/input.c`)**:
  - When two simultaneous non-stick fingers touch the viewport, track baseline distance and angle; on move, compute `intent.holo_zoom_delta` (pinch spread/pinch) and `intent.holo_orbit_dx` (2-finger twist/pan) for effortless tablet/phone Holo-Globe manipulation.
- **VR Comfort & Stereo Rendering (`src/retro_renderer.c`)**:
  - When `xr->has_eye_matrices` is true, render each eye directly using `xr->eyes[eye].proj * xr->eyes[eye].view` and position the physical 3D Left-Hand Wrist-Pad at `xr->left_hand_pos`.
  - Apply subtle dark peripheral comfort vignette bars when `xr->comfort_vignette` is active.
