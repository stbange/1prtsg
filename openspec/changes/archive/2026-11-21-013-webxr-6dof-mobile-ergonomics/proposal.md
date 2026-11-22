# Proposal: True 6-DOF WebXR Session Loop & Browser / Mobile Ergonomics (Milestone 013)

## Why
`web/xr_bridge.js` previously toggled a basic `session_active` flag without streaming per-frame `XRViewerPose` eye matrices or `XRInputSource` hand controller rays/poses into C, and mobile browsers lacked 2-finger pinch-zoom/rotate Holo-Globe gestures and responsive portrait/landscape toolbar & touch controls.

## What Changes
1. **True 6-DOF WebXR Frame Loop (`web/xr_bridge.js`, `include/rts/input.h`, `src/platform_webxr.c`, `src/retro_renderer.c`, `CMakeLists.txt`):**
   - Stream per-frame `XRViewerPose` head pose, left/right 4x4 eye view + projection matrices & viewports, left-hand 6-DOF Wrist-Pad pose, right-hand aim ray/trigger, thumbsticks, and VR comfort settings (`snap_turn_deg`, `comfort_vignette`, `seated_cockpit_mode`) into C via exported `_rts_webxr_submit_pose`.
   - Consume live eye matrices, left-hand 3D Tactical Wrist-Pad pose, right-hand laser aim ray, and VR comfort vignette inside `rts_renderer_render_webxr_stereo`.
2. **2-Finger Mobile Pinch-Zoom/Rotate & Responsive Web HUD (`include/rts/input.h`, `src/input.c`, `src/platform_sdl.c`, `web/index.html`):**
   - Track 2-finger pinch distance & angle deltas in `RtsInputRouter` (`rts_input_on_touch_down/move/up`) wired straight into `rts_hologlobe_zoom` and `rts_hologlobe_orbit_drag`.
   - Upgrade `web/index.html` with responsive portrait/landscape toolbar + Menu `[Esc]`, Board/EVA `[B]`, Time Warp `[.]`, and VR Comfort controls.
3. **Tests & Mock WebXR Injection:**
   - Add `tests/test_webxr_mobile.c`, Scenario 27, `--capture-milestone-013`, and Node.js WebXR mock frame injection check in `tools/test_wasm_node.mjs`.
