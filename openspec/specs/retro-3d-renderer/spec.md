# retro-3d-renderer Specification

## Purpose
Delivers a portable retro space-opera 3D renderer targeting OpenGL 3.3 Core, OpenGL ES 3.0, WebGL 2.0, WebXR stereo views, and a headless null backend for automated tests.

## Requirements

### Requirement: Unified Shader and Backend Abstraction
The renderer SHALL operate behind a C graphics backend interface supporting both an OpenGL 3.3 Core / ES 3.0 / WebGL 2.0 backend loading external GLSL shaders and a headless recording null backend.

#### Scenario: Rendering scene through null and OpenGL backends
- **GIVEN** a populated scene with meshes, directional sprites, voxel chunks, shield domes, and a 3D Holo-Globe
- **WHEN** a frame is rendered using either the null backend or OpenGL 3.3 Core backend
- **THEN** draw batches complete with zero errors and the OpenGL backend can export a valid PNG screenshot

### Requirement: Retro Space-Opera Visual Primitives
The renderer SHALL render camera-relative floating-origin 3D meshes with nearest-neighbor pixel textures, directional sprites, optional voxel meshes, translucent energy shield domes, glowing 3D Planetary Holo-Globes with orbital tracks, and bitmap HUD / Touch overlays.

#### Scenario: Multi-primitive retro frame composition
- **GIVEN** a camera inside a War Room looking at a 3D Planetary Holo-Globe with exterior structures visible through viewports
- **WHEN** the renderer composes a frame
- **THEN** opaque meshes, sprites, translucent shields, glowing orbital wireframe tracks, and 2D HUD elements render in proper depth order

### Requirement: Live World, Naval Carrier and Foundry Rendering
The retro 3D renderer SHALL support rendering live world entities from the Commander's camera, a Tactical Wrist-Pad diegetic HUD panel, a 1:1 Naval Fleet Carrier flight deck on curved ocean, and a 1:1 Vehicle Foundry interior assembly hall.

#### Scenario: Rendering Naval Carrier flight deck and Foundry interior
- **GIVEN** external Carrier, Foundry, and Ocean assets loaded via VFS
- **WHEN** the Naval Carrier Flight Deck and Vehicle Foundry Assembly Hall views render via OpenGL or Null backend
- **THEN** frames complete with zero GL errors and display the 1:1 structures, crew, and Tactical Wrist-Pad overlay

### Requirement: Red-Alert Boarding Interior and WebXR Stereoscopic Scenes
The retro 3D renderer SHALL support red-alert strobe Dreadnought Bridge boarding interiors with meltdown countdown telemetry and side-by-side stereoscopic WebXR VR views.

#### Scenario: Capturing Dreadnought red-alert and WebXR stereo screenshots
- **GIVEN** external models, textures, and shaders loaded via VFS
- **WHEN** `RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT` and `RTS_SCENE_WEBXR_STEREO_HOLOGLOBE` render on OpenGL or Null backend
- **THEN** both scenes complete with zero GL errors and valid visual composition
