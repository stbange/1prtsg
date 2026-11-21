# Proposal: Phase 3 — Capital Fleet Warfare, Multi-Deck Boarding & Stereoscopic WebXR

## Why
1stPRTS's signature endgame experience occurs inside 680-meter Dreadnoughts and kilometer-scale Orbital Stations: fighting multi-deck boarding actions through depressurized corridors with mag-boots, capturing an enemy flagship intact by seizing its Bridge, surviving a Critical Reactor Meltdown via a desperate Hardcore Ironman sprint to the physical Escape Pod, and commanding fleets inside stereoscopic 6-DOF WebXR.

## What Changes
- **Multi-Deck Boarding & Reactor Meltdown System (`include/rts/boarding.h`, `src/boarding.c`):**
  - Tracks active boarding assaults across decks (`RTS_DECK_HANGAR_AIRLOCK` -> `RTS_DECK_ENGINEERING` -> `RTS_DECK_BRIDGE_CIC`), attacker/defender marine counts, room decompression/vacuum hazard requiring `mag_boots`, and intact Bridge capture flipping target `faction_id`.
  - Tracks Critical Reactor Meltdown countdown timers (`reactor_meltdown_active`, `meltdown_timer_s`), emitting red-alert alarm states and destroying the vessel at `t <= 0` — immediately causing Hardcore Ironman defeat if the Commander did not eject via Escape Pod in time.
- **Stereoscopic WebXR Render Loop & Phase 3 Scenes (`include/rts/gfx.h`, `src/retro_renderer.c`, `src/platform_webxr.c`):**
  - Implements `rts_renderer_render_webxr_stereo(...)` iterating left/right eye viewports with IPD separation from `RtsXrFrameState`.
  - Adds `RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT` (`08_dreadnought_redalert_boarding.png`) and `RTS_SCENE_WEBXR_STEREO_HOLOGLOBE` (`09_webxr_stereo_vr_view.png`).
- **External Lua Boarding & Capital Assault Mission (`src/lua_vm.c`, `scripts/rules/boarding.lua`, `scripts/missions/capital_assault.lua`):**
  - Exposes `engine.boarding` bindings to Lua and implements capital assault / boarding / meltdown evacuation rules in external Lua scripts.
- **Verification:**
  - Expands Unity C tests (`tests/test_boarding.c`), Lua unit tests, and automated headless game tests (Scenarios 13–15), keeping >= 95% C11 and Lua line coverage plus live OpenGL visual screenshot verification.
