# Proposal

## Why

1stPRTS needs a verified, cross-platform C11 core engine and playable Phase 1 vertical slice ("Planetary Citadel, Dropship & Orbital Drydock") built strictly on the confirmed design pillars: exclusively first-person commander gameplay, classic bold-color retro space-opera 3D aesthetics, Hardcore Ironman stakes, true-scale spherical planets with real Keplerian orbital mechanics, zero baked assets, Lua 5.4 extensibility for the full doubled 76-construct / 12-role roster, >= 95% unit test coverage, and automated game testing.

## What Changes

- **C11 Core Engine (`include/rts/`, `src/`):**
  - 64-bit geocentric math (`Vec3d`, `Quatd`, `Mat4f`), floating-origin camera transforms, cube-sphere planetary terrain/ocean model, ENU local surface frames, atmospheric density & re-entry model, and Keplerian orbital mechanics solver + propagator.
  - Layered Virtual File System (`mods/` -> `scripts/` / `assets/` / `config/`) with path-traversal sandboxing, `mod.json` discovery, hot-reload stamps, and zero baked assets.
  - Nested `SpatialFrame` interior engine (rooms, portals/airlocks, consoles, escape pods, gravity modes, atomic frame reparenting, two-tier Interior Simulation LOD).
  - Deterministic fixed-timestep entity/RTS simulation (shields, localized subsystems, crew roles, hybrid 3D/directional-sprite LOD selector, orders, Hardcore Ironman Commander state + Escape Pod survival).
  - Optional chunked Voxel volume module with face-culled/greedy mesh generation.
  - Unified Input Router supporting Desktop KB+M, Gamepad, automatic Mobile Web Touch HUD switching, and WebXR 6-DOF stereo pose & aim-ray intents.
  - Retro Space-Opera 3D Renderer behind `GfxBackend` vtable (`gfx_gl` targeting OpenGL 3.3 Core / OpenGL ES 3.0 / WebGL 2.0 + `gfx_null` headless recording backend), loading external GLSL shaders, PNG textures/sprites, OBJ meshes, and supporting real PNG screenshot capture.
  - Embedded sandboxed Lua 5.4 VM (`engine.*` API + built-in line-coverage hook).
  - SDL3 platform adapter + Emscripten Web & WebXR JS bridge (`web/index.html`, `web/xr_bridge.js`).
- **External Lua 5.4 Gameplay Layer (`scripts/`, `mods/example_mod/`):**
  - Full doubled 76-construct catalog (24 Buildings, 10 Ground Vehicles, 10 Naval Ships, 8 Aircraft, 14 Spacecraft, 10 Space Stations) + 12 crew/personnel roles loaded dynamically via VFS.
  - Factions (*Solar Directorate* vs. *Crimson Dominion*), Hardcore Ironman rules, combat/shield rules, AI commander & astrogator helpers, diegetic Holo-Globe/Wrist-Pad UI definitions, and playable vertical-slice mission.
  - Sample mod (`mods/example_mod/`) demonstrating live VFS script & asset overriding.
- **External Art & Shader Assets (`assets/`):**
  - External MIT GLSL shaders and CC BY 4.0 retro pixel-art textures, 8-directional crew sprite sheet, low-poly `.obj` models, optional `.vxl` voxel chunk, retro bitmap font, and planet config.
- **Verification Infrastructure (`tests/`, `scripts/tests/`, `tools/`):**
  - C11 Unity unit tests targeting >= 95% line coverage enforced by `tools/check_coverage.py`.
  - Lua unit tests enforcing >= 95% Lua line coverage.
  - Automated headless game test suite exercising end-to-end gameplay, orbital ascent/docking, Ironman loss vs. escape-pod survival, hybrid crew LOD, mobile touch, WebXR rays, voxels, and mod overrides.
  - Offscreen SDL3 + OpenGL 3.3 Core visual verification & screenshot capture.

## Capabilities

### New Capabilities
- `vfs-and-mods`: External layered Virtual File System, path sandboxing, mod loading, and zero-baked-assets guarantees.
- `planet-and-orbit`: True-scale 64-bit cube-sphere planet surface, ENU tangent frames, exponential atmosphere, and Keplerian orbital mechanics.
- `interiors-and-entities`: Nested local interior reference frames, portal/airlock transitions, localized subsystems, RTS orders, hybrid crew LOD, optional voxels, and Hardcore Ironman rules.
- `lua-gameplay-layer`: Sandboxed Lua 5.4 engine bindings, full doubled 76-construct / 12-role external script catalog, and runtime mod overriding.
- `retro-3d-renderer`: Unified OpenGL 3.3 Core / ES 3.0 / WebGL 2.0 retro space-opera renderer (meshes, directional sprites, optional voxels, shields, 3D Holo-Globe, HUD/Touch overlay) + headless null backend.
- `input-touch-webxr`: Unified intent input routing with automatic Mobile Web touch control switching and WebXR 6-DOF stereo/ray support.
- `testing-and-coverage`: >= 95% C and Lua unit test coverage enforcement and automated headless + GPU game tests.

### Modified Capabilities

## Impact

- Establishes the complete CMake build system (`CMakeLists.txt`, `CMakePresets.json`), C11 core engine (`include/rts/`, `src/`), external asset & script directories (`assets/`, `scripts/`, `mods/`, `config/`), web/WebXR launcher (`web/`), and verification suites (`tests/`, `tools/`).
- Dependencies fetched cleanly via CMake `FetchContent`: SDL3 (zlib), Lua 5.4 (MIT), Unity (MIT), stb (MIT/Public Domain).
