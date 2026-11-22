# Tasks

## 1. True 6-DOF WebXR Frame Streaming & Comfort Settings
- [x] 1.1 Extend `RtsXrFrameState` in `include/rts/input.h`, add `rts_webxr_submit_pose` + comfort config in `include/rts/platform.h` / `src/platform_webxr.c`, and export `_rts_webxr_submit_pose` in `CMakeLists.txt`
- [x] 1.2 Upgrade `web/xr_bridge.js` to run `xrSession.requestAnimationFrame`, extract real `XRViewerPose` eye matrices + `XRInputSource` 6-DOF hands/rays/axes, and stream into WASM C every frame
- [x] 1.3 Upgrade `rts_renderer_render_webxr_stereo` in `src/retro_renderer.c` to consume true eye matrices, 3D left-hand Wrist-Pad pose, right-hand aim laser ray, and VR comfort vignette

## 2. Mobile 2-Finger Pinch/Rotate Holo-Globe & Adaptive Web UI
- [x] 2.1 Implement 2-finger pinch-zoom & drag-rotate gesture tracking in `src/input.c` and route into Holo-Globe in `src/platform_sdl.c`
- [x] 2.2 Upgrade `web/index.html` with Menu `[Esc]`, Board `[B]`, Time Warp `[.]`, and responsive portrait/landscape adaptive toolbar

## 3. Tests, Visual Inspection, Node.js WebXR Smoke Test & CI
- [x] 3.1 Add `tests/test_webxr_mobile.c`, Scenario 27, `--capture-milestone-013`, and Node.js WebXR mock frame injection in `tools/test_wasm_node.mjs`
- [x] 3.2 Visually inspect `m013_01..02_*.png` via `view_file`, verify >=95% coverage + 0 ASan leaks, archive OpenSpec `013`, push to `main`, and verify 5/5 CI + GitHub Pages green
