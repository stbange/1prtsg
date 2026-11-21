# Spec Delta

## ADDED Requirements

### Requirement: Live World, Naval Carrier and Foundry Rendering
The retro 3D renderer SHALL support rendering live world entities from the Commander's camera, a Tactical Wrist-Pad diegetic HUD panel, a 1:1 Naval Fleet Carrier flight deck on curved ocean, and a 1:1 Vehicle Foundry interior assembly hall.

#### Scenario: Rendering Naval Carrier flight deck and Foundry interior
- **GIVEN** external Carrier, Foundry, and Ocean assets loaded via VFS
- **WHEN** the Naval Carrier Flight Deck and Vehicle Foundry Assembly Hall views render via OpenGL or Null backend
- **THEN** frames complete with zero GL errors and display the 1:1 structures, crew, and Tactical Wrist-Pad overlay
