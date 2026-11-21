# Tasks

## 1. Cross-Platform CI & Web/WebXR Portability
- [x] 1.1 Fix `CMakeLists.txt` for Windows MSVC (`_CRT_SECURE_NO_WARNINGS`) and Emscripten (`CMAKE_C_EXTENSIONS ON` + `callMain` export)
- [x] 1.2 Wire `rts_web_is_touch_device()` into `src/platform_webxr.c`, `src/platform_sdl.c`, and update `web/index.html`

## 2. All 76 Unique Sculpted 3D Models & Multi-Deck Interiors
- [x] 2.1 Implement `tools/generate_all76_models.py` generating 76 unique sculpted Solaris `.obj` models under `assets/models/<id>.obj` and update `scripts/units/*.lua` + `manifest.json`
- [x] 2.2 Implement `scripts/interiors/catalog.lua` defining tailored multi-deck rooms, portals, and consoles for all 76 constructs and cover in `scripts/tests/unit/run_all.lua` + `scripts/tests/game/run_all.lua`
- [x] 2.3 Add dynamic per-blueprint mesh cache (`rts_renderer_get_mesh`) and 6-class 76-construct OpenGL gallery renderer (`rts_renderer_render_roster_class`, `--capture-roster-gallery`) to `include/rts/gfx.h`, `src/retro_renderer.c`, `src/platform_sdl.c`, `src/main.c`

## 3. Seamless Interactive Planetary-to-Orbital RTS Loop
- [x] 3.1 Upgrade `rts_renderer_render_live_world` and `src/platform_sdl.c` with seamless Sandbox RTS mode (`M`), live Holo-Globe orders, interior-to-exterior walking, Astrogator 400 km orbital ascent/docking, boarding launch, and `F5`/`F9` `RTS_SAVE_V1` save/load

## 4. Verification, Visual Inspection & 5-Platform CI Sign-off
- [x] 4.1 Add unit tests (`tests/test_assets_and_renderer.c`) verifying dynamic loading of all 76 unique `.obj` models, gallery rendering, and live seamless world rendering (`>= 95%` C11 & Lua coverage, `0` ASan/UBSan leaks)
- [x] 4.2 Capture and visually inspect ALL 6 class gallery contact sheets (all 76 unique models!) + live seamless RTS screenshots via `view_file` (`0` OpenGL errors)
- [x] 4.3 Archive OpenSpec `007`, push to `main`, and verify ALL 5 GitHub Actions CI jobs pass green (Linux, ASan, macOS, Windows MSVC, Emscripten Web/WebXR)
