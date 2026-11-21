# economy-sensors-and-comms Specification

## Purpose
Provides the physical resource economy, timed 1:1 construct production queues, spherical-horizon sensor Fog-of-War, and planetary-to-orbital Comms Relay network connectivity.

## Requirements

### Requirement: Resource Economy and 1:1 Production Queues
Production facilities (Foundries, Hangars, Naval Drydocks, Orbital Shipyards) SHALL deduct Ore, Fuel, and Manpower resources when queueing any of the 76 blueprints and spawn the completed physical construct once build time elapses.

#### Scenario: Queueing and completing a Plasma Heavy Tank
- **GIVEN** a Solar Directorate Vehicle Foundry with sufficient Ore, Fuel, and Power
- **WHEN** a `veh_plasma_tank` is queued and simulation advances past its build duration
- **THEN** resources are deducted and a live `veh_plasma_tank` entity spawns at the Foundry exit apron

### Requirement: Spherical Horizon Fog-of-War and Sensors
Sensor detection SHALL test range and spherical planetary horizon line-of-sight so enemy units over the planetary horizon remain hidden until spotted by friendly scouts, radar arrays, or passing Orbital Recon Satellites.

#### Scenario: Orbital recon satellite reveals over-the-horizon enemy
- **GIVEN** an enemy unit positioned beyond the surface horizon of a planetary Command Citadel
- **WHEN** an Orbital Recon Satellite orbits into line-of-sight above the enemy hemisphere
- **THEN** sensor visibility transitions from hidden to detected

### Requirement: Planetary-to-Orbital Comms Relay Network
Command link connectivity SHALL propagate through friendly Comms Towers, Naval Relay Tenders, and Orbital Comms Relay Satellites using spherical line-of-sight hops so destroying a critical relay severs remote command links.

#### Scenario: Relay satellite bridges far-side command link
- **GIVEN** a surface squad on the far side of the planet with no direct horizon line-of-sight to HQ
- **WHEN** a friendly Orbital Comms Relay Satellite has line-of-sight to both HQ and the squad
- **THEN** the squad reports linked comms status, and destroying the relay transitions it to autonomous offline status
