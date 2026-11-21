# Spec Delta

## ADDED Requirements

### Requirement: Quantitative Fun Telemetry, Bot Benchmarks and Solaris Combat Feel
The engine SHALL provide a deterministic 60 Hz live tactical skirmish simulation and non-invasive JSONL telemetry harness evaluating Novice, Human-Proxy (200 ms ± 35 ms delay), Oracle, and SpamBot personas across all six quantitative fun pillars (Kinesthetics, Flow, Strategic Entropy, Pacing, Fairness, and Loop Friction). Live combat SHALL implement 120 ms input buffering, 100 ms coyote grace, non-linear motion damping, multisensory feedback juice (hitstop, trauma-squared shake <= 4% viewport height, particles, pitch-varied SFX, UI pops), 5 synergistic tactical actions, a Wave Director with assault peaks and breather troughs, >= 300 ms enemy telegraphs, generous hurtbox/hitbox geometry, and <= 1.0 s in-memory instant retry.

#### Scenario: Running automated 4-persona Fun Audit benchmark
- **GIVEN** the Solaris patched tactical simulation with deterministic seed control
- **WHEN** `1prtsg --fun-eval --seed 42 --episodes 25` executes all four personas
- **THEN** composite Fun Score `S_fun` achieves `>= 85 / 100`, all six pillars meet their design targets, and `boot`/`peak_tension`/`game_over` live OpenGL frames render with zero GL errors and `>= 3.5:1` WCAG contrast
