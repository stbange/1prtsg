# Proposal: Quantitative Game Fun Evaluation, Kinesthetics, Pacing & Tactical Feel Overhaul

## Why
Applying the `game-fun-evaluator` skill to `1prtsg` reveals that while the core 64-bit planetary/orbital architecture, 76-construct Lua roster, and Solaris Retro-Futurism visuals are complete, the live first-person gameplay loop currently uses instantaneous boxcar movement, unbuffered inputs, zero multisensory combat juice, flat hitscan DPS timers, no Wave Director pacing, no attack telegraphing, and no instant in-memory retry.

## What Changes
1. **Non-Invasive 60 Hz Telemetry & 4-Persona Playtest Harness (`include/rts/fun_eval.h`, `src/fun_eval.c`):**
   - Implement structured JSONL telemetry logging covering lifecycle timestamps, input latency/drop tracking, entity/collision geometry, juice channels, and strategic choices.
   - Implement deterministic + generalization seeded playtest evaluation across `NoviceBot`, `HumanProxyBot` (`200 ms ± 35 ms` reaction delay), `OracleBot`, and `SpamBot`, computing all six pillar scores (`M_feel`, `M_flow`, `M_choice`, `M_pace`, `M_fair`, `M_loop`) and composite `S_fun`.
2. **Live First-Person Tactical Combat, Kinesthetics, Juice & Wave Director (`include/rts/tactical_sim.h`, `src/tactical_sim.c`, `src/controller.c`, `scripts/rules/combat.lua`, `scripts/ai/skirmish_ai.lua`):**
   - Measure an honest unpatched baseline first, then apply Remediation Playbook upgrades: `120 ms` input buffer, `100 ms` coyote grace, non-linear acceleration/deceleration + thruster dash curves, `45 ms` hitstop, trauma-squared camera shake (`<= 4%` viewport), directional particle bursts, pitch-randomized spatial SFX triggers, UI pop callouts, 5 synergistic tactical abilities (`Plasma Lance`, `EMP Pulse`, `Thruster Dash`, `Fleet Strike`, `Field Repair`), Wave Director alternating assault waves and breather troughs, `420 ms` enemy telegraphs, generous hurtbox (`0.61`) & hitbox (`1.22`) ratios, `650 ms` post-hit i-frames, and instant in-memory retry (`< 0.3 s`).
3. **Live OpenGL Combat Rendering & Visual Frame Audit (`src/retro_renderer.c`, `src/platform_sdl.c`, `src/main.c`):**
   - Render live projectiles, telegraph charge rings, particles, camera trauma shake, ability bar, wave banner, UI pop callouts, and instant-retry Game Over overlay.
   - Capture and visually audit `boot`, `peak_tension`, and `game_over` frames for WCAG contrast (`>= 3.5:1`), zero overlap/clipping, and `0` GL errors.

## Impact
- Raises `1prtsg` composite Fun Score `S_fun` from unpatched baseline into high-craft engaging gameplay (`>= 85 / 100`) while maintaining `>= 95%` C11 & Lua unit test coverage, `0` ASan/UBSan leaks, and `0` OpenGL errors.
