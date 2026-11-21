# Tasks

## 1. Core Phase 2 C11 Modules & Unit Tests (>= 95% Coverage)
- [x] 1.1 Implement `include/rts/audio.h`, `src/audio.c`, and `src/audio_sdl3.c` (3D attenuation, interior muffling, vacuum silence, WAV parser) and verify Unity unit tests pass
- [x] 1.2 Implement `include/rts/economy.h` and `src/economy.c` (resource ledger, timed 1:1 production queues, spherical horizon LOS, Fog-of-War sensors, Comms Relay BFS graph) and verify Unity unit tests pass
- [x] 1.3 Implement `include/rts/controller.h` and `src/controller.c` (live 1st-person walking, console interaction, manual piloting override, Astrogator orbital ascent/transfer/docking/shielded re-entry autopilot) and verify Unity unit tests pass
- [x] 1.4 Extend `src/lua_vm.c` with `engine.audio`, `engine.economy`, `engine.sensors`, and `engine.control` bindings and update unit tests

## 2. Combined Arms Assets, Renderer Extensions & Lua Systems
- [x] 2.1 Generate external Naval Fleet Carrier `.obj`, ocean & CRT console textures, and docking/hum `.wav` SFX via `tools/generate_retro_assets.py`
- [x] 2.2 Extend `src/retro_renderer.c` & `src/platform_sdl.c` with live-camera rendering, Tactical Wrist-Pad diegetic overlay, Naval Fleet Carrier Flight Deck scene, and Vehicle Foundry Assembly Hall scene
- [x] 2.3 Implement external Lua scripts: `scripts/rules/economy.lua`, `scripts/rules/sensors.lua`, `scripts/ai/astrogator.lua`, `scripts/ai/skirmish_ai.lua`, `scripts/ui/diegetic_ui.lua`, and `scripts/missions/combined_arms.lua`

## 3. Verification: >= 95% C/Lua Coverage, Automated Game Tests & Screenshots
- [x] 3.1 Expand C Unity unit tests, Lua unit tests, and headless automated game tests covering all Phase 2 capabilities
- [x] 3.2 Verify C coverage >= 95%, Lua coverage >= 95%, zero GL errors on new Naval Carrier & Foundry screenshots, inspect PNGs, and archive Phase 2 OpenSpec change
