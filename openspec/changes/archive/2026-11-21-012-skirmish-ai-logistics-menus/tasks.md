# Tasks

## 1. Strategic AI, Physical Logistics & 4 Curated Scenarios (Lua 5.4)
- [x] 1.1 Implement `scripts/ai/strategic_ai.lua` for autonomous multi-theater base/shipyard/relay expansion and combined-arms invasions (`balanced` default / `aggressive`)
- [x] 1.2 Implement `scripts/rules/logistics.lua` for physical Ore Cargo Trucks, Naval Relay Tenders, and Orbital Lifter convoys with raidable supply lines
- [x] 1.3 Implement `scripts/missions/scenarios.lua` with all 4 curated scenarios (Orbital Drop Defense, Carrier Strike, Escape the Flagship 60s Meltdown, Drydock Boarding)

## 2. Solaris Main Menu, Pause Menu, Match End & 5-Slot Persistent Saves (C11)
- [x] 2.1 Implement `include/rts/ui_menus.h` and `src/ui_menus.c` with Solaris Main Menu (default boot), Skirmish Configurator (Ironman ON default, Balanced AI default), 76-Construct Free-Explore Picker, Scenario Selector, 5-Slot Save/Load Browser (OS disk + Web `localStorage`), Pause Menu (`Esc`), and Victory/Defeat overlay

## 3. Renderer, Platform, Tests, Visual Inspection & CI
- [x] 3.1 Render live 3D rotating Helios Prime planet/shipyard backdrop & Solaris Menu UI in `src/retro_renderer.c`, wire interactive menu + scenario launch + persistent saves + `--capture-milestone-012` in `src/platform_sdl.c`, `src/main.c`, `include/rts/platform.h`, and `web/index.html`
- [x] 3.2 Add `tests/test_skirmish_menus.c`, Lua unit tests (`>= 95%` Lua coverage), and Automated Game Scenarios 25–26 (`>= 95%` C coverage, `0` ASan/UBSan leaks)
- [x] 3.3 Visually inspect all 4 `m012_01..04_*.png` screenshots via `view_file`, archive OpenSpec `012`, update `README.md` & `manifest.json`, push to `main`, and verify 5/5 CI + GitHub Pages green
