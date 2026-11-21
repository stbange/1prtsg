# Design: Quantitative Fun Evaluation & Solaris Tactical Game Feel

## Closed-Loop Workflow
1. **Instrument** (`src/tactical_sim.c`, `src/fun_eval.c`): Fixed `60 Hz` deterministic simulation step decoupled from wall-clock rendering (`--seed`, `--fun-eval`, `--telemetry-out`).
2. **Baseline Audit (Before)**: Run `RTS_FEEL_CONFIG_BASELINE()` representing the exact unpatched engine state (boxcar velocity, 0 ms buffer/coyote, 0 juice channels, flat linear enemy fire without telegraphs, 1.0x hurtbox, un-synergized spam weapon, slow cold-reload retry) across `N = 25` episodes per persona (`Novice`, `Human-Proxy @ 200ms ± 35ms`, `Oracle`, `SpamBot`).
3. **Remediation Patching**: Enable `RTS_FEEL_CONFIG_SOLARIS()` across live interactive play (`src/platform_sdl.c`), `RtsCommanderController` (`src/controller.c`), Lua combat/AI helpers (`scripts/rules/combat.lua`, `scripts/ai/skirmish_ai.lua`), and OpenGL live renderer (`src/retro_renderer.c`).
4. **Re-Verification (After)**: Re-run the identical `N = 25` seeded + randomized evaluation harness and capture live OpenGL `boot`, `peak_tension`, and `game_over` frames for visual & WCAG luminance contrast auditing.

## Key Engineering Contracts
- **Zero-Regression Architecture:** All existing Phase 1–4 unit tests, 18 automated Lua game tests, and 10 showcase screenshots remain intact and green.
- **Deterministic PRNG:** Local xorshift64* seeded from `--seed` ensures identical before/after comparisons plus cross-seed generalization.
- **WCAG Contrast & Camera Safety:** Foreground projectiles/reticles/telegraphs use high-luminance Solaris coral/gold/cyan against deep slate/twilight backdrops (`>= 3.5:1`), and trauma shake amplitude is strictly clamped to `<= 2.6%` of viewport height (`< 4%` limit).
