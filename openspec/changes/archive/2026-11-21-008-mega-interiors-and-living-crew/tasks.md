# Tasks

## 1. Mega-Interior Floorplan Generator & Multi-Room Traversal
- [x] 1.1 Extend `include/rts/interior.h` with `RTS_CONSOLE_TURBOLIFT`, room metadata (`room_kind`, `door_mask`, `deck_index`, `damaged_pct`, `breached`), and fix `src/controller.c` so Commander walks seamlessly across connected rooms through open doorways
- [x] 1.2 Implement `include/rts/interior_gen.h` and `src/interior_gen.c` generating deterministic 1–420 room multi-deck floorplans for all 76 constructs with BFS connectivity verification and active-deck streaming (`rts_mega_hydrate_deck`, `rts_mega_switch_deck`)
- [x] 1.3 Wire Lua bindings (`engine.interior.mega_stats`) in `src/lua_vm.c` and verify 1–420 room connectivity across all 76 constructs in Lua unit & automated game tests

## 2. Living 12-Role Crew Simulation
- [x] 2.1 Implement `include/rts/crew.h` and `src/crew_sim.c` populating every active-deck room across all 12 crew roles with uniform colors, console work, multi-room corridor patrol, engineer damage-control pathing, and Red Alert / counter-boarding drill state

## 3. Room Architecture Rendering, Deck Blueprint Minimap & Browser Polish
- [x] 3.1 Upgrade `src/retro_renderer.c` to render enclosed ceilings, vaulted arches, open doorway bulkheads, room-specific 3D props (Holo-Globe, Reactor Tokamak, Hangar Starfighters, Turbolift Shaft, Medbay Pods, Armory, Escape Pods), all staffed 12-role crew agents, and a live Diegetic Deck Blueprint Minimap
- [x] 3.2 Upgrade `src/platform_sdl.c`, `src/main.c`, and `web/index.html` with live mega-ship cycling (`N`), deck turbolift controls (`[`/`]` + `E`), Red Alert crew drill (`F`), `--capture-mega-interiors`, and browser Pointer Lock + WebAudio unlock

## 4. Verification, Visual Audit & 5-Platform CI + GitHub Pages Deploy
- [x] 4.1 Add C unit tests (`tests/test_mega_interiors_and_crew.c`) covering all 76 constructs' floorplan connectivity (up to 420 rooms), multi-room walking, deck switching, and 12-role crew AI (`97.26%` C11 coverage, `>= 95%` Lua coverage, `0` ASan leaks)
- [x] 4.2 Capture & visually inspect 4 live OpenGL 3.3 Core mega-interior & staffed crew screenshots via `view_file` (`0` GL errors)
- [x] 4.3 Archive OpenSpec `008`, push to `main`, verify 5/5 CI jobs + Node.js WASM smoke test + GitHub Pages deployment green
