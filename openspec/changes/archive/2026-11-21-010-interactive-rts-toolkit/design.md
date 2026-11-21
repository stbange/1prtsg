# Design: 010 — Interactive First-Person RTS Toolkit

## Module Breakdown
- `include/rts/hologlobe_ui.h` & `src/hologlobe_ui.c`: Pure deterministic C11 state & 3D math for the Tier 3 Planetary & Orbital Holo-Globe (`RtsHoloGlobeState`). Zero heap leaks, stack-safe (embedded inside heap-allocated `InteractiveApp`), shared across mouse, touch, WebXR rays, and Lua headless tests.
- `include/rts/field_gear.h` & `src/field_gear.c`: Pure deterministic C11 state for Tier 1 Tactical Wrist-Pad (4 tabs), Optical Binoculars (`1x`/`4x`/`12x`) + 3D Laser Designator raycast, 7-Order Radial Quick Wheel, and Tier 2 Facility Build Console (`RtsFacilityConsoleState`, dynamically filtering all 76 blueprints by facility class).
- `src/retro_renderer.c`:
  - Stores optional non-owning pointers `active_holo`, `active_gear`, `active_eco` via `rts_renderer_set_rts_toolkit(...)`.
  - Renders 3D Holo-Globe unit blips (Fog-of-War filtered), selection halos, Keplerian orbital tracks, order vectors, and holographic building ghost on the War Room Holo-Globe.
  - Renders live 1:1 progressive assembly scaffolds + rising sculpted `.obj` hulls + golden welding arcs for active `RtsProductionJob`s next to their producer facility.
  - Renders crisp 1.0x pixel-font diegetic HUD overlays for the 4-Tab Wrist-Pad, Optical Binoculars reticle + laser beacon, 7-segment Radial Wheel, Tier 3 Holo-Globe toolbar, and Tier 2 Facility Build Terminal.
- `src/lua_vm.c` & `scripts/ui/diegetic_ui.lua`: Exposes `engine.rts` table (`hologlobe_pick`, `hologlobe_place`, `designate`, `radial_order`, `facility_queue`) for Lua scripting & Node.js WASM end-to-end verification.
- `src/platform_sdl.c` & `web/index.html`: Wires `Tab`, `Z`, `V`/RMB, `E` (Holo-Globe / Factory Terminal), scroll/keys, `--capture-rts-toolkit`, and web toolbar shortcuts.
