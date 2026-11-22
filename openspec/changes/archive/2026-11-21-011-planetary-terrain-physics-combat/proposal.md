# Proposal: True Spherical Planetary Terrain, Oceans, 6-Class Vehicle Physics & Unified Live Combat (Milestone 011)

## Why
The live 64-bit planetary-to-orbital sandbox currently renders a single static `terrain_island.obj` near `(0, R, 0)`, uses a generic velocity vector for manual piloting across all vehicle classes, and keeps high-craft combat mechanics isolated inside Tactical Defense (`tactical_sim.c`) rather than unified inside the live 64-bit `RtsWorld`.

## What Changes
1. **Continuous Spherical Quadtree Terrain & Animated Ocean (`include/rts/planet_mesh.h`, `src/planet_mesh.c`):**
   - Multi-LOD cube-sphere terrain patch & animated ocean mesh generator driven by `rts_planet_terrain_height`, smoothly blending from 1m boots-on-ground terrain/coastline/waves up to full planetary sphere + atmospheric scattering + cloud deck from 400 km–36,000 km orbit.
2. **Class-Specific Manual Piloting & 6-DOF Flight (`include/rts/vehicle_physics.h`, `src/vehicle_physics.c`):**
   - Distinct physics across all 6 classes: Ground Vehicles (surface-normal alignment, slope handling, hover-cushion, independent turret yaw/pitch), Naval Ships & Submarines (ocean buoyancy/wave roll + submarine ballast dive/surface depth), Aircraft & Dropships (VTOL hover ↔ forward flight aerodynamics + functional cargo bay clamping to haul a Ground Vehicle to orbit), and Spacecraft/Capital Ships (6-DOF Newtonian thrusters, prograde/retrograde burns, re-entry plasma heat flux, docking clamps, plus safe single-player Time Compression `1x`/`2x`/`4x`/`8x`).
3. **Unified Live-World Combat & Destructible Voxel Craters (`include/rts/combat_sim.h`, `src/combat_sim.c`):**
   - Unify Mode `L` Solaris combat feel (EMP-Prime x2.4 synergy, plasma bolts, continuous beam lances, homing torpedoes, AA flak, orbital ion cannons, directional hex-shield ripples) directly into the 64-bit `RtsWorld` across all 76 constructs.
   - Add physical compartment hitboxes routing impacts into `RTS_SUBSYS_*` + interior damage states (`RtsMegaInterior` room damage for Engineers to repair), plus optional destructible voxel crater/asteroid carving via `src/voxel.c`.
4. **Renderer, Lua Bindings, Tests & Visual Captures:**
   - Wire spherical terrain/ocean, vehicle physics, cargo bay clamping, time compression, unified live combat VFX, and voxel craters into `src/retro_renderer.c`, `src/platform_sdl.c`, `src/lua_vm.c`, `src/main.c` (`--capture-milestone-011`), `web/index.html`, C/Lua tests, and Automated Game Scenarios 23–24.
