# planet-and-orbit Specification

## Purpose
Models true-scale 64-bit spherical planets, local East-North-Up surface frames, exponential atmospheres, and authentic Keplerian orbital mechanics for planetary-to-orbital gameplay.

## Requirements

### Requirement: True-Scale Spherical Planet Surface
The planetary model SHALL represent a full 64-bit spherical world using a 6-face cube-sphere parameterization supporting deterministic terrain elevation, curved oceans, and local East-North-Up (ENU) tangent frames.

#### Scenario: Computing ENU surface frame on spherical planet
- **GIVEN** a spherical planet with radius 6,371,000 meters
- **WHEN** a building is placed at a given latitude and longitude
- **THEN** its local Up axis aligns radially outward from planet center and East, North, Up form an orthonormal right-handed basis

### Requirement: Exponential Atmosphere and Re-entry Model
The engine SHALL compute altitude-dependent atmospheric density and aerodynamic drag/thermal heating that drops smoothly to vacuum above the atmospheric ceiling.

#### Scenario: Transition from sea level to orbital vacuum
- **GIVEN** a planet with sea-level atmospheric density and scale height
- **WHEN** density is sampled at 0m, 40km, and 250km altitude
- **THEN** density decreases exponentially with altitude and reaches zero in orbital vacuum

### Requirement: Keplerian Orbital Mechanics
The orbital module SHALL compute classical Keplerian orbital elements from 64-bit state vectors, propagate orbital motion under central-body gravity, sample 3D orbit trajectories for Holo-Globes, and solve Hohmann transfer burn delta-v.

#### Scenario: Circular orbit stability and element recovery
- **GIVEN** a spacecraft at 400km circular equatorial orbital velocity
- **WHEN** orbital elements are solved and the state is propagated for 60 simulation seconds
- **THEN** eccentricity remains near zero and orbital altitude stays within stable bounds
