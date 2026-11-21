# Proposal

## Why

Phase 1 delivered the foundational C11 engine, doubled 76-construct / 12-role Lua catalog, retro 3D renderer, and >= 95% coverage gates. To make 1stPRTS genuinely playable across land, sea, air, and orbit, Phase 2 adds live first-person Commander locomotion and direct piloting override, true orbital ascent/transfer/docking/re-entry autopilot, physical economy & production queues, true sensor Fog-of-War & orbital Comms Relay networks, SDL3 spatial audio with interior muffling and orbital vacuum acoustics, live world rendering (including Naval Fleet Carrier & Foundry interiors), and multi-theater Lua Skirmish AI.

## What Changes

- **C11 Core Engine (`include/rts/`, `src/`):**
  - `src/audio.c` + `include/rts/audio.h`: `AudioBackend` vtable (`audio_null` headless recorder + `audio_sdl3` real SDL3 audio stream mixer loading external `.wav` assets), 3D distance attenuation, interior acoustic muffling, and vacuum silence above the atmospheric ceiling.
  - `src/economy.c` + `include/rts/economy.h`: Resource ledger (`ore`, `fuel`, `power`, `manpower`), timed 1:1 production queues attached to Factories, Foundries, Naval Drydocks, and Orbital Shipyards, plus physical Sensor Fog-of-War and Comms Relay graph connectivity (linking Comms Towers, Naval Relay Tenders, and LEO/GEO Relay Satellites).
  - `src/controller.c` + `include/rts/controller.h`: Live 1st-person Commander walking inside rooms & across planetary ENU surfaces, console interaction (`Holo-Globe`, `Factory Terminal`, `Pilot Seat`, `Bridge Helm`, `Escape Pod`), direct manual piloting override for Ground/Naval/Air/Space constructs with atmospheric lift/drag & orbital torch thrust, and automated Astrogator orbital ascent/transfer/docking/de-orbit flight autopilot.
  - Extended `src/retro_renderer.c` & `src/lua_vm.c`: Live world entity & projectile rendering, Naval Fleet Carrier flight deck & Vehicle Foundry interior views, Tactical Wrist-Pad diegetic overlay, and new `engine.audio`, `engine.economy`, `engine.sensors`, and `engine.control` Lua bindings.
- **External Lua 5.4 Gameplay Layer (`scripts/`):**
  - `scripts/rules/economy.lua`, `scripts/rules/sensors.lua`, `scripts/ai/astrogator.lua`, `scripts/ai/skirmish_ai.lua`, `scripts/ui/diegetic_ui.lua`, and `scripts/missions/combined_arms.lua`.
- **External Assets (`assets/`):**
  - Naval Fleet Carrier `.obj` model (`assets/models/carrier.obj`), ocean texture (`assets/textures/ocean_wave.png`), CRT console texture (`assets/textures/crt_console.png`), and engine-hum / docking `.wav` SFX.
- **Tests & Coverage:**
  - Full Unity C unit tests for `audio`, `economy`, `controller`, and updated `lua_vm`/`retro_renderer` keeping >= 95% C line coverage.
  - Expanded Lua unit tests (>= 95% Lua coverage) and headless automated game tests covering 1st-person walking/piloting, orbital ascent/re-entry autopilot, production queues, Fog-of-War/Comms Relay blackout, spatial audio/vacuum silence, and Skirmish AI.

## Capabilities

### New Capabilities
- `first-person-and-piloting`: Live 1st-person Commander locomotion, console interaction, direct manual piloting override, and Astrogator orbital autopilot (ascent, transfer, docking, shielded re-entry).
- `economy-sensors-and-comms`: Resource economy, timed 1:1 construct production queues, physical sensor Fog-of-War, orbital recon sweeps, and planetary-to-orbital Comms Relay connectivity.
- `spatial-audio-acoustics`: Hardware-abstracted 3D spatial audio mixer with interior hull muffling, vacuum silence in orbit, and external `.wav` playback.

### Modified Capabilities
- `lua-gameplay-layer`: Expose `engine.audio`, `engine.economy`, `engine.sensors`, and `engine.control` bindings plus multi-theater Skirmish AI and diegetic UI Lua scripts.
- `retro-3d-renderer`: Add live world entity/projectile rendering, Naval Fleet Carrier flight deck, Vehicle Foundry 1:1 assembly hall, and Tactical Wrist-Pad diegetic panel.

## Impact

- Adds 3 new pure-C core modules (`audio.c`, `economy.c`, `controller.c`) to `rts_core` and `audio_sdl3.c` to `rts_platform`, preserving hardware abstraction seams and >= 95% line coverage.
- Zero baked assets; all new models, textures, audio `.wav` files, and Lua scripts remain external and indexed in `manifest.json`.
