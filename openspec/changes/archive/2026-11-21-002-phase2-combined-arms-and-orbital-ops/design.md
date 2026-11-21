# Design

## Context

See `proposal.md` and `docs/ARCHITECTURE.md`. Phase 2 elevates the Phase 1 engine core into a live multi-theater combined-arms and orbital simulation while strictly preserving the C/Lua split, zero baked assets, and >= 95% unit test coverage.

## Goals / Non-Goals

**Goals:**
- Provide deterministic 1st-person Commander movement, room bounds clamping, console interaction, and seamless manual-piloting vs. AI-crew handoff.
- Implement automated orbital ascent, Keplerian circularization, docking rendezvous, and shielded atmospheric re-entry with thermal shield load.
- Model true planetary-to-orbital Comms Relay connectivity (BFS/graph across ground towers, naval relay tenders, and orbiting relay satellites with spherical planet line-of-sight horizon occlusion!).
- Abstract SDL3 audio playback behind `AudioBackend` so all 3D attenuation, interior muffling, and orbital vacuum acoustics are 100% unit-testable via `audio_null`.

**Non-Goals:**
- Multi-deck boarding combat & WebXR dual-eye rendering (scoped to Phase 3).
- Runtime hot-reload file watching & save/load serialization (scoped to Phase 4).

## Decisions

1. **Spherical Planet Horizon Occlusion for Sensors & Comms Relays:**
   - *Rationale:* On a true spherical planet ($R = 6,371\text{ km}$), two points have direct line-of-sight iff the segment connecting them clears the planetary sphere (`min_dist_to_origin >= R`). That single geometric test naturally makes Comms Towers, Naval Relay Tenders, and LEO/GEO Relay Satellites strategically meaningful!
2. **Unified Piloting & Astrogator Controller (`src/controller.c`):**
   - *Rationale:* Separates Commander on-foot walking (`MODE_ON_FOOT`), console operation (`MODE_AT_CONSOLE`), direct vehicle/ship/aircraft/spacecraft piloting (`MODE_PILOTING`), and AI astrogator orbital maneuvers into clean testable state transitions.
3. **Audio Backend Vtable (`AudioBackend`):**
   - *Rationale:* `audio.c` computes 3D gain, stereo pan, interior acoustic low-pass muffle, and vacuum attenuation in pure C before forwarding voice commands to `audio_null` (tests) or `audio_sdl3` (runtime).

## Risks / Trade-offs

- **[Risk] Orbital ascent/de-orbit timescale vs. fast unit/game tests** -> Mitigation: Space-opera high-thrust fusion drives (35–120 m/s^2 acceleration) plus deterministic step counts keep orbital burns crisp, responsive, and fast to verify in automated tests.
