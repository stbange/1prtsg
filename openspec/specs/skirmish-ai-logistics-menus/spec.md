# Spec: Full Skirmish Campaign, Strategic AI Commander, Physical Logistics & Solaris Menu System

## ADDED Requirements

### Requirement: Autonomous Multi-Theater Strategic AI Commander
The Lua gameplay layer SHALL provide `scripts/ai/strategic_ai.lua` enabling enemy factions to autonomously evaluate economy, expand mines, foundries, spaceports, orbital shipyards, Recon Satellites, and GEO Comms Relays (rebuilding severed relays), build balanced multi-theater forces, and execute combined-arms surface and orbital invasions under Balanced or Aggressive pacing.

### Requirement: Physical Logistics Convoy Network
The Lua gameplay layer SHALL provide `scripts/rules/logistics.lua` physically dispatching Ore Cargo Trucks, Naval Relay Tenders, and Orbital Cargo Lifters between mines, refineries, depots, and orbital drydocks so escorting friendly convoys and raiding hostile convoys directly affects faction resource delivery.

### Requirement: Solaris Main Menu, 4 Curated Scenarios & Persistent 5-Slot Saves
The engine SHALL boot into the Solaris Main Menu over a live 3D rotating planet and orbital shipyard backdrop, offering one-click entry into Planetary-to-Orbital Skirmish (Hardcore Ironman ON by default, toggleable; Balanced AI default), Free-Explore Sandbox (all 76 constructs), Tactical Defense, 4 Curated Scenarios (Orbital Drop Defense, Carrier Strike, Escape the Flagship 60s Meltdown, Drydock Boarding), Pause Menu (`Esc`), Match Victory/Defeat summary, and 5 persistent save slots stored on disk (Desktop) and `localStorage` (Web).
