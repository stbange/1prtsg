# Proposal: Full Skirmish Campaign, Autonomous Strategic AI, Physical Logistics, Solaris Main Menu & Persistent Saves (Milestone 012)

## Why
Players currently boot directly into a single live mode without a Solaris Title / Main Menu, scenario browser, skirmish configurator, multi-slot persistent save browser, physical resource hauling convoys, or a full multi-theater Strategic AI Commander capable of expanding bases, orbital shipyards, satellite relay networks, and combined-arms invasions.

## What Changes
1. **Autonomous Multi-Theater Strategic AI (`scripts/ai/strategic_ai.lua`):**
   - Evaluates faction economy, builds mines/refineries/foundries/spaceports/orbital shipyards, launches Recon & GEO Comms Relays (severable by the player!), constructs balanced ground/naval/air/orbital task forces, and launches amphibious & orbital dropship assaults with configurable Balanced (default) vs Aggressive pacing.
2. **Physical Logistics Network (`scripts/rules/logistics.lua`):**
   - Ore Cargo Trucks (`veh_cargo_truck`), Naval Relay Tenders (`ship_relay_tender`), and Orbital Cargo Lifter Tugs (`air_heavy_lifter` / `spc_cargo_tug`) physically shuttle ore & deuterium between mines, refineries, depots, and orbital drydocks — making convoy escort and commerce raiding genuine strategic gameplay.
3. **4 Curated Scenarios (`scripts/missions/scenarios.lua`):**
   - **Orbital Drop Defense**, **Carrier Strike**, **Escape the Flagship (60s Ironman Meltdown)**, and **Drydock Boarding**.
4. **Solaris Main Menu, Skirmish Setup, Pause Menu & Persistent 5-Slot Saves (`include/rts/ui_menus.h`, `src/ui_menus.c`, `src/retro_renderer.c`, `src/platform_sdl.c`, `web/index.html`):**
   - Default startup screen: live 3D rotating Helios Prime planet & orbital station backdrop with the **Solaris Main Menu** (Planetary-to-Orbital Skirmish with Ironman ON default + Balanced AI default, Free-Explore Sandbox for all 76 constructs, Tactical Defense, 4 Curated Scenarios, 5-Slot Save/Load Browser backed by OS files on Desktop + `localStorage` on GitHub Pages Web, and Pause/Victory/Defeat overlays).
