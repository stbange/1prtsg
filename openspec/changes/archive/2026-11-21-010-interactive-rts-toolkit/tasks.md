# Tasks

## 1. Tier 3 Interactive 3D Holo-Globe
- [x] 1.1 Implement `include/rts/hologlobe_ui.h` and `src/hologlobe_ui.c` with 3D globe raycast unit picking (obeying spherical-horizon Fog of War), spin/zoom, box selection, all 7 orbital/surface orders, and validated 3D placement of all 24 Planetary Buildings & 10 Space Stations

## 2. Tier 1 Field Gear & Tier 2 Facility Consoles
- [x] 2.1 Implement `include/rts/field_gear.h` and `src/field_gear.c` for the 4-tab Tactical Wrist-Pad, 4x/12x Optical Binoculars & 3D Laser Designator, 7-order Radial Quick Wheel, and class-filtered Tier 2 Facility Build Terminal
- [x] 2.2 Expand `scripts/ui/diegetic_ui.lua` and expose `engine.rts.*` Lua bindings in `src/lua_vm.c`

## 3. 3D Holographic Rendering, 1:1 Gantry Assembly & Interactive Controls
- [x] 3.1 Upgrade `include/rts/gfx.h` and `src/retro_renderer.c` to render 3D Holo-Globe blips/tracks/ghosts, 1:1 progressive gantry assembly for `RtsProductionJob`s, optical zoom FOV + laser beacon, 4-tab Wrist-Pad, Radial Wheel, and Tier 2/3 console overlays
- [x] 3.2 Wire all Tier 1/2/3 controls, `--capture-rts-toolkit` (4 OpenGL screenshots), and web toolbar controls into `src/platform_sdl.c`, `src/main.c`, `include/rts/platform.h`, and `web/index.html`

## 4. Tests, Visual Inspection, Coverage & 5-Platform CI + GitHub Pages
- [x] 4.1 Add `tests/test_rts_toolkit.c`, Lua unit tests, and Automated Game Scenarios 21–22 (`>= 95%` C & Lua coverage, `0` ASan/UBSan leaks)
- [x] 4.2 Capture & visually inspect all 4 `toolkit_01..04_*.png` OpenGL 3.3 Core screenshots via `view_file` (`0` GL errors)
- [x] 4.3 Archive OpenSpec `010`, update `README.md` & `manifest.json`, push to `main`, and verify 5/5 CI + Node.js WASM smoke test + GitHub Pages green
