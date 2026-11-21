# Design

## Context

See `proposal.md` for motivation and `docs/ARCHITECTURE.md` for system diagrams. This change establishes the full Phase 1 C11 engine foundation, external Lua 5.4 gameplay layer with the doubled 76-construct / 12-role roster, cross-platform SDL3/OpenGL/Emscripten/WebXR plumbing, and automated testing + >= 95% coverage gates.

## Goals / Non-Goals

**Goals:**
- Deliver a compiling, cleanly tested C11 core engine with strict `-Wall -Wextra -Werror` discipline.
- Keep 100% of game assets, shaders, and Lua scripts outside the binary in sibling directories (`assets/`, `scripts/`, `mods/`, `config/`).
- Achieve >= 95% C line coverage and >= 95% Lua line coverage via hardware abstraction vtables (`GfxBackend`, `FsBackend`, `XrBackend`) and headless deterministic test runners.
- Render live, visually verified retro space-opera 3D screenshots via SDL3 + OpenGL 3.3 Core demonstrating the Command Citadel War Room 3D Holo-Globe, 1:1 planetary surface & energy shields, enterable Dropship & hybrid crew, true-scale orbital shipyard & dreadnought above the curved planetary limb, and Mobile Web Auto-Touch HUD.

**Non-Goals:**
- Multi-machine network synchronization (deferred to a future OpenSpec change; deterministic core preserves compatibility).
- Full voice acting or multi-hour story campaign scripting in Phase 1.

## Decisions

1. **Unified OpenGL 3.3 Core / ES 3.0 / WebGL 2.0 Shader Core:**
   - *Rationale:* Covers Windows, Linux, macOS, Emscripten Web, and WebXR with a single C renderer (`src/gfx_gl.c` + `src/retro_renderer.c`). External GLSL files omit `#version`; the loader prepends `#version 330 core` on desktop or `#version 300 es\nprecision highp float;` on GLES3/WebGL2.
   - *Alternatives considered:* Maintaining separate Vulkan/Metal/WebGPU backends in Phase 1 (rejected due to complexity; `GfxBackend` vtable preserves future extensibility).
2. **Dependency-Injected Hardware Seams for >= 95% Coverage:**
   - *Rationale:* All math, planetary/orbital physics, VFS, interiors, ECS/RTS simulation, voxels, input/touch/XR intent routing, Lua bindings, and high-level retro scene assembly call pure C APIs or `GfxBackend` / `FsBackend` vtables, allowing exhaustive native unit tests with `gfx_null` and mock filesystems.
3. **64-Bit Geocentric Coordinates + Floating-Origin Render Matrices:**
   - *Rationale:* Planetary radii (~6,371 km) and orbits (~42,000 km) require `double` precision (`Vec3d`). Subtracting `cam_world_d` on CPU before casting to `float` eliminates GPU jitter across surface-to-orbit transitions.
4. **Lua 5.4 Sandboxed VFS Loader + Full Doubled Roster (`76` Constructs, `12` Roles):**
   - *Rationale:* Keeps all unit/building/interior/role metadata editable by players and modders. Every construct in `docs/ROSTER.md` is registered at startup via `scripts/units/registry.lua` and can be inspected, spawned, entered, and overridden by `mods/`.
5. **External Loose-File Web Asset Delivery:**
   - *Rationale:* On Emscripten, `web/index.html` reads sibling `manifest.json` and fetches loose `assets/`, `scripts/`, `mods/`, and `config/` files via HTTP into Emscripten's runtime MEMFS before starting `main`, ensuring zero assets are baked into `.wasm`.

## Risks / Trade-offs

- **[Risk] Software OpenGL (Mesa llvmpipe) differences vs. hardware GPUs** -> Mitigation: Stick strictly to standard OpenGL 3.3 Core / ES 3.0 features, check `glGetError()` after every frame in visual tests, and inspect captured PNG screenshots directly.
- **[Risk] Coverage dilution from platform-only SDL3/Emscripten glue** -> Mitigation: Keep `platform_sdl.c` and `gfx_gl.c` ultra-thin adapters while putting all scene graph, batching, camera, HUD, touch geometry, and state logic into pure platform-independent C modules (`retro_renderer.c`, `input.c`, `world.c`, `interior.c`, `planet.c`, `vfs.c`, `voxel.c`, `lua_vm.c`).
