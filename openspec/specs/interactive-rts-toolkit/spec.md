# Specification: Interactive First-Person RTS Toolkit (Tier 1 / Tier 2 / Tier 3)

## ADDED Requirements

### Requirement: Tier 3 Interactive 3D Holo-Globe Picking, Orders & Placement
The engine SHALL provide a 3D raycast-interactive Holo-Globe controller (`RtsHoloGlobeState`) projecting live friendly constructs and Fog-of-War detected hostile contacts onto the spinning holographic sphere and orbital rings.
#### Scenario: Raycast Selection & Arbitrary Order Issuance
- **WHEN** a Commander raycasts onto a friendly construct blip on the 3D Holo-Globe and issues a Surface Move, Attack, Orbit, Hohmann, Dock, Re-entry, or Boarding order
- **THEN** the target unit is selected cleanly, Fog-of-War hides undetected horizon-occluded enemies, and the issued order updates `RtsEntity` / `RtsWorld` / `RtsBoardingSystem` state deterministically.
#### Scenario: 3D Holographic Building & Station Placement
- **WHEN** the Commander selects any of the 24 Planetary Buildings or 10 Space Stations in placement mode and confirms a valid surface or orbital target coordinate
- **THEN** placement validation checks faction resources and altitude rules, deducts cost from `RtsEconomySystem`, and spawns the construct with its own enterable spatial frame at the exact 64-bit world coordinates.

### Requirement: Tier 1 Personal Field Gear (Wrist-Pad, Optical Designator, Radial Wheel)
The engine SHALL provide diegetic first-person field gear controllable on desktop KBM, mobile touch, and WebXR.
#### Scenario: 4-Tab Tactical Wrist-Pad with Horizon Fog-of-War & Comms Status
- **WHEN** the Commander toggles the Tactical Wrist-Pad (`Tab`)
- **THEN** the renderer displays selectable `Tactical Radar` (filtering contacts via `rts_sensors_is_target_detected` and displaying Comms Relay link status), `Deck Map`, `Fleet Roster`, and `Economy & Production` tabs.
#### Scenario: Optical Binoculars (4x/12x) & 3D Laser Designator
- **WHEN** the Commander cycles Optics (`Z`) and paints a target in the world
- **THEN** the live camera FOV narrows to 4x (`20°`) or 12x (`7.5°`), raycasts against live constructs and planetary terrain, places a 3D laser target beacon, and supports instant squad attack/move designation.
#### Scenario: 7-Order Radial Quick Wheel
- **WHEN** the Commander opens the Radial Quick Order Wheel (`V` / Right-Click) and activates `Escort`, `Move`, `Attack`, `Orbit`, `Dock`, `Board`, or `Evacuate`
- **THEN** the corresponding RTS command executes immediately for selected/nearby units or the Commander's vessel.

### Requirement: Tier 2 Interactive Facility Build Console & 1:1 Progressive Assembly
Accessing a Foundry, Coastal Dock, Hangar, or Orbital Shipyard console SHALL open a class-filtered build menu and render active `RtsProductionJob` items progressively assembling at 1:1 scale on the live gantry/apron before rolling out.
#### Scenario: Facility Build Queue & 1:1 Gantry Rollout
- **WHEN** the Commander queues a construct from a facility console
- **THEN** `RtsEconomySystem` tracks build progress, the renderer draws the 1:1 holographic scaffold and rising sculpted `.obj` hull with weld beams, and completion spawns the enterable construct cleanly.
