# Design: 76 Unique Sculpted Models, Multi-Deck Interiors, Seamless RTS & 5-Platform CI

## 1. Cross-Platform CI
- `CMakeLists.txt`: Use `CMAKE_C_EXTENSIONS ON` when `EMSCRIPTEN` is set (required by SDL3 `EM_ASM`), define `_CRT_SECURE_NO_WARNINGS` on `MSVC`, and include `'callMain'` in Emscripten `EXPORTED_RUNTIME_METHODS`.
- `src/platform_webxr.c` / `src/platform_sdl.c`: Expose `rts_web_is_touch_device()` so browser Auto-Touch and the "Toggle Touch HUD" button dynamically switch `RtsInputRouter` in `interactive_tick`.

## 2. 76 Unique Sculpted `.obj` Models & Dynamic Renderer Mesh Cache
- Build `tools/generate_all76_models.py` extending the Solaris `SculptBuilder` (parametric lofts, swept diamond airfoils, chamfered hulls, domes, torus rings, spires, trusses, hydrofoils, parabolic dishes, rail barrels, emissive accents `rgb > 1.08`).
- Generate 76 unique files `assets/models/<construct_id>.obj` (24 Buildings, 10 Ground Vehicles, 10 Naval Ships, 8 Aircraft, 14 Spacecraft, 10 Space Stations) while retaining legacy base `.obj` files for existing showcase compatibility.
- Update `scripts/units/*.lua` so each construct sets `mesh = "models/<id>.obj"`.
- Extend `RtsRetroRenderer` (`include/rts/gfx.h`, `src/retro_renderer.c`) with a 96-slot external VFS mesh cache (`rts_renderer_get_mesh(rr, rel_path)`) that loads each construct's unique `.obj` on demand and frees all cached GPU meshes cleanly on shutdown.
- Add `rts_renderer_render_roster_class` + `--capture-roster-gallery` to render 6 crisp 1280×720 OpenGL 3.3 Core class gallery sheets displaying all 76 unique sculpted constructs side-by-side with labels.

## 3. 76 Tailored Multi-Deck Enterable Interiors
- Create `scripts/interiors/catalog.lua` providing deterministic multi-deck room, portal, and console layouts tailored to every one of the 76 constructs (respecting `rooms` count, dimensions, planetary vs artificial gravity, War Room Holo-Globes, Foundry Terminals, Pilot Seats/Bridge Helms, Engineering Reactors, Hangars, and Escape Pod Bays) and expose a helper `hydrate_construct(entity_id, bp_id)` exercised in Lua unit & automated game tests.

## 4. Seamless Interactive Planetary-to-Orbital RTS Mode
- Add `RTS_PLAY_SANDBOX_RTS` (toggled with `M`) in `src/platform_sdl.c` + `rts_renderer_render_live_world` in `src/retro_renderer.c`:
  - Populates Helios Prime with the enterable Command Citadel, Vehicle Foundry, parked Dropship Valkyrie-2, Grav-Plasma Tank, hostile Crimson Tank, and 400 km LEO Orbital Shipyard + Crimson Dreadnought.
  - Supports walking interiors, `E` console/portal interaction, Holo-Globe live commands (`[1]` Queue Tank, `[2]` Order Attack, `[3]` Astrogator 400 km Ascent & Shipyard Dock, `[4]` Launch Boarding Assault), direct Dropship piloting, altitude-reactive sky dome (surface twilight -> upper atmosphere -> deep orbital space), and `F5`/`F9` deterministic `RTS_SAVE_V1` save/load.
