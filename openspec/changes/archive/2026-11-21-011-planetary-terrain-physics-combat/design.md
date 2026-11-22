# Design: Spherical Planetary Terrain, 6-Class Vehicle Physics & Unified Live Combat

## Module Breakdown
- **`include/rts/planet_mesh.h` & `src/planet_mesh.c`**:
  - `rts_planet_select_lod(altitude_m)`: computes adaptive patch span (`280 m` near ground up to `800 km` in orbital view) and LOD level (`0..4`).
  - `rts_planet_build_surface_patch(...)`: samples `rts_planet_terrain_height` on a tangent-plane grid projected onto the true spherical planet surface (`R = 6,371 km`), outputting vertex-colored Solaris terrain triangles (coastal ivory beaches, glacier-slate highlands, emerald tundra, alpine snowpeaks) relative to the Commander camera (camera-relative `float32` vertices for zero floating-point jitter!).
  - `rts_planet_build_ocean_patch(...)`: generates animated spherical wave-ripple ocean triangles at `sea_level_m` with dynamic vertex shimmer and coastal foam edges.
  - `rts_planet_build_orbital_clouds(...)`: generates translucent spherical cloud-band vertices at `altitude = 18 km` for orbital perspective.
- **`include/rts/vehicle_physics.h` & `src/vehicle_physics.c`**:
  - `RtsVehiclePhysicsState`: tracks per-craft class dynamics (hover height, slope alignment quaternion, turret yaw/pitch, wave roll/pitch, submarine ballast depth `[-120 m, 0 m]`, VTOL blend `[0..1]`, airspeed, clamped cargo vehicle ID, 6-DOF Newtonian velocity, prograde/retrograde burn telemetry, re-entry heat flux `W/m^2` + glow `[0..1]`, and safe single-player `time_compression` `1x/2x/4x/8x`).
  - `rts_vehicle_physics_step(...)`: integrates class-accurate physics on any entity in `RtsWorld` (handling Ground, Naval/Submarine, Aircraft/Dropship VTOL, and Spacecraft 6-DOF orbital mechanics seamlessly).
  - `rts_vehicle_clamp_cargo(...)` / `rts_vehicle_release_cargo(...)`: locks a friendly Ground Vehicle inside a Dropship's vehicle bay so it rides along seamlessly from planetary surface to 400 km LEO orbit and back!
  - `rts_time_compression_set(...)`: permits `1x`, `2x`, `4x`, `8x` compression when safe out of active combat, automatically clamping back to `1x` if projectiles or enemies are in combat range.
- **`include/rts/combat_sim.h` & `src/combat_sim.c`**:
  - `RtsCombatSim`: manages up to `128` live 64-bit projectiles (`PLASMA_BOLT`, `BEAM_LANCE`, `HOMING_TORPEDO`, `AA_FLAK`, `ORBITAL_ION_CANNON`, `EMP_PULSE`), directional shield impact ripples (`RtsShieldRipple`), EMP-Prime status timers per entity (x2.4 damage multiplier on primed targets), compartment hitbox mapping (`rts_combat_hit_subsystem` mapping local hit vector to `REACTOR`/`ENGINES`/`WEAPONS`/`SENSORS`/`BRIDGE` and propagating physical interior damage to `RtsMegaInterior`), and voxel crater carving (`rts_combat_carve_voxel_crater`).
  - `rts_combat_step(...)`: runs deterministic AI weapon firing across all 76 constructs based on class & range, projectile ballistics/homing, EMP-Prime synergy, shield ripples, compartment damage, and voxel crater excavation.
