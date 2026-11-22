# Tasks

## 1. Spherical Quadtree Terrain & Animated Oceans
- [x] 1.1 Implement `include/rts/planet_mesh.h` and `src/planet_mesh.c` for camera-relative multi-LOD spherical terrain patches, animated specular oceans, and orbital cloud layers from `rts_planet_terrain_height`

## 2. 6-Class Vehicle Physics, Dropship Cargo Clamping & Time Compression
- [x] 2.1 Implement `include/rts/vehicle_physics.h` and `src/vehicle_physics.c` covering Ground slope/turret, Naval buoyancy & Submarine dive depth, Aircraft/Dropship VTOL + vehicle bay cargo clamping, Spacecraft 6-DOF orbital burns & re-entry plasma heat, and safe `1x/2x/4x/8x` time compression

## 3. Unified Live-World Combat & Voxel Destruction
- [x] 3.1 Implement `include/rts/combat_sim.h` and `src/combat_sim.c` unifying EMP-Prime x2.4 synergy, 6 weapon families, directional hex-shield impact ripples, physical compartment hitboxes -> `RtsMegaInterior` room damage, and destructible voxel crater carving

## 4. Renderer, Lua, Interactive Controls, Tests & Visual Verification
- [x] 4.1 Wire `planet_mesh`, `vehicle_physics`, `combat_sim`, voxel craters, `--capture-milestone-011` (4 OpenGL captures), and Lua `engine.combat.*` / `engine.physics.*` bindings into `src/retro_renderer.c`, `src/platform_sdl.c`, `src/lua_vm.c`, `src/main.c`, `include/rts/gfx.h`, `include/rts/platform.h`, and `web/index.html`
- [x] 4.2 Add `tests/test_planet_physics_combat.c`, Lua unit tests, and Automated Game Scenarios 23–24 (`>= 95%` C & Lua coverage, `0` ASan/UBSan leaks)
- [x] 4.3 Visually inspect all 4 `m011_01..04_*.png` screenshots via `view_file`, archive OpenSpec `011`, update `README.md` & `manifest.json`, push to `main`, and verify 5/5 CI + GitHub Pages green
