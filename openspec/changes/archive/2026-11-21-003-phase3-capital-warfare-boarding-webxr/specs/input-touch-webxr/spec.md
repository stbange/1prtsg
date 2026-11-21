# Spec Delta

## ADDED Requirements

### Requirement: Stereoscopic Dual-Eye WebXR Viewport Rendering
The renderer SHALL render dual-eye side-by-side stereoscopic views driven by `RtsXrFrameState` eye viewports and IPD offsets alongside visible 3D controller laser aim-rays intersecting the Planetary Holo-Globe.

#### Scenario: Rendering stereoscopic WebXR Holo-Globe frame
- **GIVEN** an active WebXR stereo state with left/right eye offsets and a controller aim-ray hitting the Holo-Globe
- **WHEN** `rts_renderer_render_webxr_stereo` renders the frame
- **THEN** both left and right eye viewports render cleanly with zero GL errors and visible stereo parallax
