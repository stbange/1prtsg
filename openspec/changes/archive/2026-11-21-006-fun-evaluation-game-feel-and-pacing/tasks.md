# Tasks

## 1. Instrumentation & Baseline Fun Audit
- [x] 1.1 Implement `include/rts/tactical_sim.h` and `src/tactical_sim.c` (live 60 Hz tactical skirmish loop supporting both unpatched baseline profile and patched Solaris Game-Feel profile)
- [x] 1.2 Implement `include/rts/fun_eval.h` and `src/fun_eval.c` (JSONL telemetry logger, 4 synthetic bot personas with 200ms ± 35ms Human-Proxy delay, exact 6-pillar scoring & Fun Audit Report)
- [x] 1.3 Wire `--fun-eval`, `--seed`, `--episodes`, `--telemetry-out`, `--baseline`, and `--capture-fun-frames` into `src/main.c` and `src/platform_sdl.c`
- [x] 1.4 Run honest BASELINE audit (`N = 25` episodes per persona) and document real bottlenecks

## 2. Targeted Code Remediations (Game Feel, Synergy, Pacing, Fairness & Retry)
- [x] 2.1 Upgrade `src/controller.c` with non-linear acceleration/damping & `120 ms` interact/action input buffer, and upgrade `scripts/rules/combat.lua` + `scripts/ai/skirmish_ai.lua` with EMP-Prime synergy and Wave Director hooks
- [x] 2.2 Upgrade `src/platform_sdl.c` interactive controls (continuous `SDL_GetKeyboardState` movement, 5 tactical action bindings, instant in-memory `R`/`Enter` retry after `250 ms` guard)
- [x] 2.3 Upgrade `src/retro_renderer.c` live world renderer with trauma camera shake, projectiles, `420 ms` telegraph charge rings, directional particle bursts, ability cooldown bar, wave banner, UI pop callouts, and instant-retry Game Over overlay

## 3. Re-Verification, Visual Audit & Archiving
- [x] 3.1 Re-run `N = 25` Fun Audit on patched Solaris engine (deterministic seed + randomized generalization seeds) and verify all 6 pillars improve cleanly without regressions
- [x] 3.2 Capture real-time OpenGL screenshots at `boot`, `peak_tension`, and `game_over`, verify `0` GL errors, measure WCAG luminance contrast (`>= 3.5:1`), and inspect every screenshot via `view_file`
- [x] 3.3 Add unit tests (`tests/test_fun_eval.c`) ensuring `>= 95%` C11 & Lua coverage, `0` ASan/UBSan leaks, archive OpenSpec `006`, and push to `main`
