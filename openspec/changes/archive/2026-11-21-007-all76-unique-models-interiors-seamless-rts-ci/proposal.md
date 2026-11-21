# Proposal: All 76 Unique Sculpted 3D Models & Interiors, Seamless Interactive RTS Loop, and 5-Platform Green CI

## Why
1. GitHub Actions CI currently passes on Linux GCC, Linux ASan/UBSan, and macOS Clang, but fails on Windows MSVC (C4996 CRT deprecation warnings under `/WX`) and Emscripten Web/WebXR (`CMAKE_C_EXTENSIONS OFF` breaks SDL3's `EM_ASM`).
2. While all planetary, interior, economy, astrogator, boarding, and save/load subsystems work and pass tests, interactive play needs a unified seamless Planetary-to-Orbital RTS mode connecting interior walking, Holo-Globe orders, Dropship ascent/docking, and `F5`/`F9` save/load live in one session.
3. All 76 constructs in `scripts/units/*.lua` currently share 10 base `.obj` meshes; giving **every single one of the 76 constructs** its own unique sculpted Solaris Retro-Futurism `.obj` model and tailored multi-deck enterable interior blueprint dramatically fulfills the game's 1:1 monumental identity.

## What Changes
1. **Cross-Platform CI & Web/WebXR Polish (`CMakeLists.txt`, `src/platform_webxr.c`, `src/platform_sdl.c`, `web/index.html`):**
   - Enable `_CRT_SECURE_NO_WARNINGS` on MSVC, `CMAKE_C_EXTENSIONS ON` on Emscripten, export `callMain`, and wire `rts_web_is_touch_device()` directly into the live input router.
2. **All 76 Unique Sculpted Solaris `.obj` Models & 76 Tailored Multi-Deck Interiors (`tools/generate_all76_models.py`, `assets/models/*.obj`, `scripts/units/*.lua`, `scripts/interiors/catalog.lua`, `src/retro_renderer.c`):**
   - Procedurally sculpt 76 distinct, non-blocky, multi-material vertex-colored Solaris Retro-Futurism `.obj` models (24 Buildings, 10 Ground Vehicles, 10 Naval Ships, 8 Aircraft, 14 Spacecraft, 10 Space Stations), map every Lua unit blueprint to its dedicated `models/<id>.obj`, and add a dynamic per-blueprint mesh cache in `RtsRetroRenderer`.
   - Define tailored multi-deck room/portal/console interior templates for all 76 constructs in external Lua (`scripts/interiors/catalog.lua`) with deterministic hydration into `RtsInteriorSystem`.
   - Add `--capture-roster-gallery` to render live OpenGL 3.3 Core showcase contact sheets of all 76 unique models across all 6 classes.
3. **Seamless Interactive Planetary-to-Orbital RTS Sandbox (`src/platform_sdl.c`, `src/retro_renderer.c`, `src/controller.c`):**
   - Wire seamless live mode (`M`): walk Citadel War Room -> interact with 3D Holo-Globe to issue live build/attack/ascent/boarding orders -> walk out to Dropship -> pilot or launch Astrogator 400 km orbital ascent -> dock with Orbital Shipyard -> `F5`/`F9` save/load snapshots, rendering every live entity with its unique blueprint `.obj` mesh.

## Impact
- All 5 CI platforms green (Linux, ASan, macOS, Windows MSVC, Emscripten Web/WebXR).
- 76/76 unique external `.obj` models + 76/76 enterable multi-deck interiors with zero baked assets.
- `>= 95%` C11 & Lua unit test coverage, `0` ASan/UBSan leaks, and `0` OpenGL errors across all showcase and gallery screenshots.
