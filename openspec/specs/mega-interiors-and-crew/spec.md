# Specification: Mega-Interiors & Living 12-Role Crew

## Requirements

### Requirement: Scale-Authentic Multi-Deck Room Counts Across All 76 Constructs
Every construct blueprint MUST generate a deterministic, 100%-connected multi-deck floorplan scaled to its real metric dimensions and class:
- Small single-crew craft: 1–2 rooms (1 deck)
- Medium vehicles/ships/buildings: 4–14 rooms (1–2 decks)
- Large cruisers/destroyers/citadels: 24–72 rooms (3–6 decks)
- Colossal capital ships & space stations: 120–420+ rooms across 8–20 decks (e.g., Fleet Carrier `>= 140` rooms, Fleet Command Dreadnought `>= 260` rooms, Orbital Shipyard Drydock `>= 400` rooms across 20 decks).

#### Scenario: BFS Connectivity Verification Across All 76 Blueprints
- **WHEN** `rts_mega_generate_for_blueprint` generates floorplans for all 76 blueprints
- **THEN** every deck's rooms MUST form a single connected component reachable via open bulkhead doorways from the deck's entry/turbolift room, with valid consoles matching `has_holo_globe` and `has_escape_pod`.

### Requirement: Seamless Multi-Room Traversal & Multi-Deck Turbolift Streaming
The Commander MUST be able to walk smoothly through open doorways from room to room on the active deck without being trapped in a single room AABB, and travel vertically between Deck 1 and Deck N via Turbolift stations with active-deck hydration.

#### Scenario: Walking Across Adjacent Rooms & Switching Decks
- **WHEN** the Commander walks across a shared bulkhead doorway from Bridge into Corridor on Deck 1 and activates the Turbolift to Deck 14 (Reactor Deck)
- **THEN** the Commander transitions seamlessly between room bounds on Deck 1, and switching to Deck 14 re-hydrates Deck 14's rooms, consoles, and staffed crew cleanly in `RtsInteriorSystem`.

### Requirement: Full Room-by-Room Staffing Across All 12 Crew Roles
Every hydrated deck MUST be staffed with live `RtsCrewAgent` NPCs drawn from the 12 crew roles, exhibiting station work, corridor patrol, engineer damage control when a room is damaged, and Red Alert battle-station / counter-boarding behavior.

#### Scenario: Dreadnought & Shipyard Full Staffing
- **WHEN** Deck 1 (Bridge/CIC) or Deck 14 (Reactor Core) of a capital construct is hydrated
- **THEN** every populated room spawns role-appropriate crew agents (Officers, Pilots, Engineers, Marines, Medics, Gunners, etc.), Engineers path toward damaged rooms to repair them, and Marines patrol connected corridors.

### Requirement: Room-Specific 3D Dressing, Diegetic Deck Map & Browser Ergonomics
The renderer MUST draw doorway-pierced bulkheads, role-distinct 3D room props, role-colored 3D crew agents, and a Diegetic Deck Blueprint Minimap; the web shell MUST support Pointer Lock mouse-look and WebAudio unlock on first click.
