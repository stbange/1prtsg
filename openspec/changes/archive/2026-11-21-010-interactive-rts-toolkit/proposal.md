# Proposal: 010 — Full Interactive First-Person RTS Toolkit (Tier 1 / Tier 2 / Tier 3)

## Why
Mode `M` (Seamless Planetary-to-Orbital Sandbox RTS) currently exposes only 4 hardcoded Holo-Globe hotkeys (`1`–`4`), with no 3D raycast picking/orbit-dragging on the spinning Holo-Globe, no interactive 3D base/station placement, no functional Tier 1 Personal Field Gear (Tactical Wrist-Pad, Optical Binoculars & Laser Designator, Radial Quick Order Wheel), and no live 1:1 facility gantry assembly when queuing constructs from Factory/Drydock/Hangar consoles.

## What Changes
1. **Tier 3 — Interactive 3D Planetary & Orbital Holo-Globe (`include/rts/hologlobe_ui.h`, `src/hologlobe_ui.c`):**
   - True 3D raycast picking onto the spinning holographic sphere and Keplerian orbital rings (desktop cursor, touch tap, WebXR ray).
   - Smooth globe spin & zoom (`36,000 km` GEO down to surface theater), unit/formation selection, Fog-of-War filtered blips, arbitrary order issuance (`Move`, `Attack`, `Orbit`, `Hohmann Transfer`, `Rendezvous & Dock`, `Shielded Re-entry`, `Launch Boarding`), and live 3D holographic ghost placement for all 24 Planetary Buildings & 10 Space Stations.
2. **Tier 1 — Personal Field Gear (`include/rts/field_gear.h`, `src/field_gear.c`, `scripts/ui/diegetic_ui.lua`):**
   - **Tactical Wrist-Pad (`Tab`):** 4 interactive diegetic tabs (`Tactical Radar` obeying spherical-horizon Fog of War + Comms Relays, `Deck Map`, `Fleet Roster`, `Economy & Production`).
   - **Optical Binoculars & Laser Designator (`Z`):** True optical FOV zoom (`1x` -> `4x` -> `12x`) painting 3D world targets for artillery/orbital strikes, dropship LZs, and direct squad attacks.
   - **Radial Quick Order Wheel (`V` / Right-Click):** 7 fast 1st-person tactical orders (`Escort`, `Move`, `Attack`, `Orbit`, `Dock`, `Board`, `Evacuate`) aimed along the Commander's line of sight.
3. **Tier 2 — Interactive Facility Consoles & 1:1 Progressive Assembly (`src/field_gear.c`, `src/retro_renderer.c`):**
   - Accessing any Factory/Foundry/Hangar/Shipyard console opens a class-tailored build menu and renders queued `RtsProductionJob` constructs **assembling progressively at 1:1 scale** on the gantry/drydock apron (cyan wireframe scaffold + rising sculpted `.obj` hull + golden weld arcs) before launching into the live world.
4. **Lua Bindings, Web Controls & Verification (`src/lua_vm.c`, `src/platform_sdl.c`, `web/index.html`, `tests/test_rts_toolkit.c`):**
   - Expose `engine.rts.*` Lua bindings, add `--capture-rts-toolkit` for 4 live OpenGL 3.3 Core screenshots (`0` GL errors), add C11 + Lua unit tests and Automated Game Test Scenarios 21–22 (`>= 95%` C & Lua coverage, `0` ASan leaks, 5/5 CI + GitHub Pages WASM green).
