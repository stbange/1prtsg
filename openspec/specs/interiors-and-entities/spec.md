# interiors-and-entities Specification

## Purpose
Manages 1:1 human-scale enterable interiors via nested local reference frames, portal transitions, localized subsystems, hybrid crew LOD, optional voxels, RTS orders, and Hardcore Ironman Commander rules.

## Requirements

### Requirement: Nested Local Reference Frames
Every enterable building, vehicle, naval ship, aircraft, spacecraft, and space station SHALL own a local spatial frame capable of nesting inside parent frames and transferring occupants across airlock or ramp portals atomically.

#### Scenario: Commander boards moving dropship and docks at station
- **GIVEN** a Commander standing outside a Dropship whose parent is a Planet frame
- **WHEN** the Commander crosses the Dropship ramp portal and the Dropship later docks inside an Orbital Shipyard
- **THEN** the Commander simulates stably in the Dropship local frame and can cross the docking airlock into the Orbital Shipyard frame without NaN or clipping

### Requirement: Localized Subsystems and Crew Repair
Every operational construct SHALL track shield strength and localized subsystem integrity (reactor, engines, sensors, weapons, bridge) so localized damage degrades performance until crew engineers repair it.

#### Scenario: Engine room damage degrades speed until repaired
- **GIVEN** a ship with active shields and operational engines
- **WHEN** enemy plasma fire depletes shields and damages the engine subsystem
- **THEN** maximum propulsion speed drops proportionally until assigned engineer crew restores engine integrity

### Requirement: Hybrid Crew Visual LOD
Crew entities SHALL select low-poly 3D mesh rendering inside interiors, in WebXR, and within close range, and switch smoothly to 8- or 16-directional sprite rendering at medium and far exterior distances.

#### Scenario: Distance-based hybrid LOD switching
- **GIVEN** a crew member standing 10m away inside a Citadel and another standing 120m away outside
- **WHEN** hybrid crew LOD is evaluated
- **THEN** the interior crew member selects 3D mesh mode and the distant exterior crew member selects directional sprite mode with an azimuth frame index

### Requirement: Hardcore Ironman Commander Rule
By default, Commander death MUST immediately end the match in defeat unless the Commander reaches an operational Escape Pod or safe vessel before hull destruction.

#### Scenario: Defeat on Commander loss vs survival via escape pod
- **GIVEN** an active match under Hardcore Ironman rules
- **WHEN** the Commander's health reaches zero aboard a destroyed ship
- **THEN** the match state transitions to Defeat, whereas ejecting via an Escape Pod prior to destruction preserves Commander survival

### Requirement: Optional Chunked Voxel Volumes
The engine SHALL provide an optional chunked voxel module capable of editing voxel cells and generating face-culled triangle meshes compatible with the standard retro 3D renderer.

#### Scenario: Face-culled voxel chunk meshing
- **GIVEN** a 16x16x16 voxel chunk containing two adjacent solid blocks
- **WHEN** the voxel mesher builds geometry
- **THEN** shared internal faces are culled and only exposed exterior faces are emitted
