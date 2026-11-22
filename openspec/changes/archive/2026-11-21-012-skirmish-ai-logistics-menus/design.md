# Design: Strategic AI, Physical Logistics, 4 Scenarios, Solaris Main Menu & Multi-Slot Persistence

## Architecture
- **`scripts/ai/strategic_ai.lua`**:
  - `M.new_commander(faction_id, difficulty)` (`"balanced"` | `"aggressive"`)
  - `M.step(ai, world_ctx, dt_s)`: evaluates ore/fuel income, expands infrastructure (`bldg_ore_mine` -> `bldg_vehicle_foundry` -> `bldg_spaceport` -> `stn_orbital_shipyard`), deploys `spc_recon_sat` and `stn_comms_relay` (detects if relay is destroyed and prioritizes restoration!), queues combined-arms forces, and issues amphibious/orbital invasion orders.
- **`scripts/rules/logistics.lua`**:
  - `M.new_network()`: registers physical convoy routes (`SURFACE_TRUCK`, `NAVAL_TENDER`, `ORBITAL_LIFTER`) between producer and depot entities.
  - `M.step(net, dt_s)`: moves live convoy units physically back and forth between source & destination; delivers ore/fuel payloads upon arrival; supports `M.raid_convoy(net, route_idx)` when hostile units destroy a hauler, slashing supply delivery until escorted/replaced!
- **`scripts/missions/scenarios.lua`**:
  - Catalog of 4 curated playable scenarios (`orbital_drop_defense`, `carrier_strike`, `escape_flagship_60s`, `drydock_boarding`) with deterministic setup metadata, briefing text, starting craft, objectives, and meltdown/boarding triggers.
- **`include/rts/ui_menus.h` & `src/ui_menus.c`**:
  - `RtsMenuState`: manages `RTS_MENU_MAIN`, `RTS_MENU_SKIRMISH_SETUP`, `RTS_MENU_SCENARIOS`, `RTS_MENU_SANDBOX_PICKER`, `RTS_MENU_SAVE_LOAD`, `RTS_MENU_MODS`, `RTS_MENU_PAUSE`, `RTS_MENU_MATCH_END`, and `RTS_MENU_NONE` (in-game).
  - Defaults: `screen = RTS_MENU_MAIN`, `skirmish_ironman = true` (toggleable), `skirmish_ai_aggressive = false` (Balanced default), `planet_radius_preset = 1` (`6,371 km`).
  - 5-slot persistent save system (`rts_menu_save_slot` / `rts_menu_load_slot`): serializes `RTS_SAVE_V1` snapshots to in-memory slot metadata + OS files (`saves/slot_<N>.rts`) on Desktop and browser `localStorage` (`1prtsg_save_slot_<N>`) on Emscripten Web via `EM_ASM` bridge!
  - Full mouse click hit-testing (`rts_menu_click`) AND keyboard navigation (`Up/Down/Enter/1-6/Esc`) so both desktop and mobile/web players click straight into any mode effortlessly.
