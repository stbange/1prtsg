# Design: Phase 3 — Capital Warfare, Multi-Deck Boarding & Stereoscopic WebXR

## Architecture
- `RtsBoardingSystem` (`include/rts/boarding.h`, `src/boarding.c`) lives in `rts_core` as a deterministic fixed-timestep subsystem decoupled from platform graphics/audio:
  - Up to `RTS_MAX_BOARDING_OPS` simultaneous boarding operations on multi-deck entities (`RTS_CLASS_SPACECRAFT`, `RTS_CLASS_SPACE_STATION`, `RTS_CLASS_NAVAL_SHIP`, `RTS_CLASS_BUILDING`).
  - Each operation tracks `target_entity_id`, `attacker_faction`, `current_deck` (`0=Hangar/Airlock`, `1=Engineering`, `2=Bridge/CIC`), `attackers`, `defenders`, `decompressed` (vacuum breach), `capture_progress` (`0..1`), and state (`ACTIVE`, `CAPTURED`, `REPELLED`, `ABORTED_DESTROYED`).
  - Stepping a boarding op resolves deterministic marine-vs-defender attrition; when `decompressed == true`, unmag-booted defenders or attackers suffer extra vacuum attrition unless stabilized; clearing defenders advances decks and completing `BRIDGE_CIC` transfers `target->faction_id = attacker_faction` intact without destroying the hull!
  - `RtsMeltdownEvent` tracks critical reactor meltdowns (`entity_id`, `countdown_s`, `active`, `detonated`). Stepping decrements `countdown_s`; if `countdown_s <= 0`, calls `rts_world_destroy_entity(w, entity_id)` which triggers Hardcore Ironman defeat if the Commander is still aboard (and survival if the Commander launched an Escape Pod first).
- **Stereoscopic WebXR Rendering (`src/retro_renderer.c`):**
  - `rts_renderer_render_webxr_stereo(RtsRenderer *r, const RtsXrFrameState *xr, RtsDemoScene scene, uint32_t full_w, uint32_t full_h)` splits the framebuffer into side-by-side left/right eye viewports (`[0, w/2]` and `[w/2, w/2]`), computes left/right eye camera offsets (`±ipd/2`), renders the 3D scene from each eye including a glowing 3D WebXR controller aim-ray intersecting the Planetary Holo-Globe, and draws stereo VR diegetic HUD readouts.
- **External Lua Gameplay (`scripts/rules/boarding.lua`, `scripts/missions/capital_assault.lua`):**
  - Scripts orchestrate capital fleet engagements, launch boarding shuttles, resolve deck-by-deck capture, and run the reactor meltdown evacuation race via `engine.boarding`.
