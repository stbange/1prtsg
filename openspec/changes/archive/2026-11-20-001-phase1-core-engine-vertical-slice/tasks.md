# Tasks

## 1. Doubled Roster & Build/OpenSpec Scaffolding
- [x] 1.1 Document the complete doubled roster (76 constructs + 12 roles) in `docs/ROSTER.md` and sync `docs/CONCEPT.md`
- [x] 1.2 Create cross-platform `CMakeLists.txt`, `CMakePresets.json`, and `tools/check_coverage.py` fetching SDL3, Lua 5.4, Unity, and stb cleanly; verify native configure succeeds

## 2. Core C11 Engine Modules & Unit Tests (>= 95% Coverage)
- [x] 2.1 Implement `math3d` (64-bit `Vec3d`, `Quatd`, `Mat4f`, floating origin, raycasts) and verify Unity unit tests pass with >= 95% coverage
- [x] 2.2 Implement `planet` (cube-sphere surface, ENU frames, exponential atmosphere, Keplerian orbital elements, orbital propagation & Hohmann solver) and verify unit tests pass
- [x] 2.3 Implement `vfs` (layered `mods/` -> `scripts/`/`assets/` mounts, path-traversal sandbox, `mod.json` parsing, hot-reload) and verify unit tests pass
- [x] 2.4 Implement `interior` (nested `SpatialFrame` tree, rooms, consoles, escape pods, portal/airlock atomic reparenting, two-tier interior simulation LOD) and verify unit tests pass
- [x] 2.5 Implement `world` (deterministic ECS, shields, localized subsystems, RTS orders, crew roles, hybrid 3D/sprite LOD, Hardcore Ironman & Escape Pod survival) and verify unit tests pass
- [x] 2.6 Implement optional `voxel` chunk & face-culled mesher module and verify unit tests pass
- [x] 2.7 Implement `input` unified intent router (Desktop KB+M, Gamepad, Auto-Touch Mobile HUD layout/hit-testing, WebXR 6-DOF pose/ray mapping) and verify unit tests pass
- [x] 2.8 Implement `retro_renderer` + `gfx_null` + `gfx_gl` (OpenGL 3.3 Core / ES 3.0 / WebGL 2.0, external GLSL shaders, PNG textures/sprites, OBJ loader, Holo-Globe, shields, HUD/Touch overlay, PNG screenshot capture) and verify unit tests pass against `gfx_null`
- [x] 2.9 Implement sandboxed `lua_vm` (`engine.*` API + Lua line-coverage collector) and verify C-to-Lua binding unit tests pass
- [x] 2.10 Implement `platform_sdl`, `platform_webxr`, `main.c`, and Emscripten Web/WebXR launcher (`web/index.html`, `web/xr_bridge.js`) and verify native build compiles cleanly with zero warnings

## 3. External Assets, Full Doubled Lua Scripts & Sample Mod
- [x] 3.1 Generate external CC BY 4.0 retro pixel textures, 8-directional crew sprite sheet, retro bitmap font, low-poly `.obj` models, optional `.vxl` voxel chunk, `.wav` SFX, planet config, and MIT GLSL shaders under `assets/` + `config/` + `manifest.json`
- [x] 3.2 Implement external MIT Lua 5.4 gameplay scripts under `scripts/` registering all 76 constructs + 12 roles, factions, Hardcore Ironman rules, combat/shields, AI, diegetic UI, and vertical-slice scenario
- [x] 3.3 Create `mods/example_mod/` overriding a unit definition and texture asset to verify live VFS mod layering

## 4. Verification: >= 95% Coverage, Automated Game Tests & Real Screenshots
- [x] 4.1 Run full C Unity unit test suite with `--coverage` and verify `tools/check_coverage.py` passes >= 95% C line coverage (97.20%)
- [x] 4.2 Run full Lua unit test suite with `--lua-coverage` and verify >= 95% Lua line coverage (99.53%)
- [x] 4.3 Run headless automated game test suite covering Holo-Globe production/orders, orbital ascent & station docking, Ironman defeat vs. Escape Pod survival, hybrid crew LOD & localized repair, mobile touch Holo-Globe, WebXR stereo/ray, voxels, and mod VFS override
- [x] 4.4 Run real SDL3 + OpenGL 3.3 Core visual smoke test & screenshot capture under Xvfb/Mesa, verify zero GL errors, inspect all rendered PNG screenshots, and archive the completed OpenSpec change
