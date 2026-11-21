# Spec Delta

## ADDED Requirements

### Requirement: Red-Alert Boarding Interior and WebXR Stereoscopic Scenes
The retro 3D renderer SHALL support red-alert strobe Dreadnought Bridge boarding interiors with meltdown countdown telemetry and side-by-side stereoscopic WebXR VR views.

#### Scenario: Capturing Dreadnought red-alert and WebXR stereo screenshots
- **GIVEN** external models, textures, and shaders loaded via VFS
- **WHEN** `RTS_SCENE_DREADNOUGHT_BOARDING_REDALERT` and `RTS_SCENE_WEBXR_STEREO_HOLOGLOBE` render on OpenGL or Null backend
- **THEN** both scenes complete with zero GL errors and valid visual composition
