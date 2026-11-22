# Spec: Spherical Planetary Terrain, 6-Class Vehicle Physics & Unified Live Combat

## ADDED Requirements

### Requirement: Continuous Multi-LOD Spherical Planetary Terrain & Animated Ocean
The engine SHALL dynamically generate camera-relative spherical terrain tiles and animated ocean waves around the Commander from `rts_planet_terrain_height`, smoothly transitioning from 1m boots-on-ground relief up to full spherical curvature, atmospheric halo, and cloud deck in LEO/GEO orbit without floating-point jitter.

### Requirement: Class-Specific Manual Piloting, Cargo Bay Clamping & Safe Time Compression
The engine SHALL simulate distinct physical flight and movement envelopes for Ground Vehicles (surface-normal alignment, hover suspension, independent turret aim), Naval Ships & Submarines (wave buoyancy/roll, ballast dive depth), Aircraft/Dropships (VTOL hover to forward flight + vehicle bay cargo clamping to transport ground vehicles to orbit), and Spacecraft/Capital Ships (6-DOF Newtonian thrusters, orbital burns, re-entry plasma heat flux, plus safe `1x`/`2x`/`4x`/`8x` time compression that auto-resets to `1x` during combat).

### Requirement: Unified 64-Bit Live-World Combat & Destructible Voxel Craters
The engine SHALL run unified real-time combat inside `RtsWorld` across all 76 constructs featuring plasma bolts, continuous beam lances, homing torpedoes, AA flak, orbital ion cannons, EMP-Prime (`x2.4` synergy), directional hex-shield impact ripples, physical compartment hitboxes propagating subsystem hits into `RtsMegaInterior` room damage, and destructible voxel crater/asteroid carving.
