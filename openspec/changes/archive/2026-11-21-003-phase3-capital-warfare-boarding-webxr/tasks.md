# Tasks

## 1. Multi-Deck Boarding & Reactor Meltdown Core (`rts_core`)
- [x] 1.1 Implement `include/rts/boarding.h` and `src/boarding.c` (multi-deck breach, decompression hazard, Bridge intact capture, Critical Reactor Meltdown countdown + Hardcore Ironman interaction)
- [x] 1.2 Add `tests/test_boarding.c` covering boarding capture, repelled assault, decompression hazard, reactor meltdown Ironman defeat, and escape-pod survival

## 2. Stereoscopic WebXR & Phase 3 Visual Scenes
- [x] 2.1 Extend `include/rts/gfx.h` and `src/retro_renderer.c` with `rts_renderer_render_webxr_stereo`, `RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT`, and `RTS_SCENE_WEBXR_STEREO_HOLOGLOBE`
- [x] 2.2 Wire Phase 3 screenshot outputs (`08_dreadnought_redalert_boarding.png`, `09_webxr_stereo_vr_view.png`) in `src/platform_sdl.c` and test on Null backend in `tests/test_assets_and_renderer.c`

## 3. Lua Bindings, External Capital Assault Scripts & Automated Verification
- [x] 3.1 Expose `engine.boarding` in `src/lua_vm.c` and create `scripts/rules/boarding.lua` and `scripts/missions/capital_assault.lua`
- [x] 3.2 Expand Lua unit tests and automated game tests (Scenarios 13–15), verify >= 95% C & Lua coverage, capture & visually inspect screenshots `08` and `09`, and archive Phase 3 OpenSpec change
