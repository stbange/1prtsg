# input-touch-webxr Specification

## Purpose
Provides a unified input intent router that adapts seamlessly across Desktop Keyboard/Mouse, Gamepad, automatic Mobile Web Touch controls, and 6-DOF WebXR sessions.

## Requirements

### Requirement: Automatic Mobile Touch Control Switching
The input system SHALL detect touch or coarse-pointer environments and automatically switch to `MOBILE_TOUCH` mode with virtual thumbstick, look-drag, action cluster, and Holo-Globe tap/pinch gesture processing.

#### Scenario: Mobile touch activation and thumbstick intent
- **GIVEN** an engine initialized on a coarse-pointer mobile web device
- **WHEN** a touch begins and drags inside the left virtual thumbstick zone
- **THEN** input mode is `MOBILE_TOUCH` and normalized forward/strafe movement axes reflect the thumbstick vector

### Requirement: WebXR Stereoscopic Pose and Aim-Ray Routing
The input and XR bridge SHALL accept 6-DOF head/eye transforms and controller aim-rays to drive dual-eye stereoscopic rendering and diegetic Holo-Globe / cockpit ray interaction.

#### Scenario: Selecting a unit on the Holo-Globe via WebXR controller ray
- **GIVEN** an active WebXR session with left/right eye views and a right-hand aim-ray pointing at a Holo-Globe target
- **WHEN** the controller primary trigger is pressed
- **THEN** the engine resolves the ray intersection and dispatches a unit selection or order intent

### Requirement: Stereoscopic Dual-Eye WebXR Viewport Rendering
The renderer SHALL render dual-eye side-by-side stereoscopic views driven by `RtsXrFrameState` eye viewports and IPD offsets alongside visible 3D controller laser aim-rays intersecting the Planetary Holo-Globe.

#### Scenario: Rendering stereoscopic WebXR Holo-Globe frame
- **GIVEN** an active WebXR stereo state with left/right eye offsets and a controller aim-ray hitting the Holo-Globe
- **WHEN** `rts_renderer_render_webxr_stereo` renders the frame
- **THEN** both left and right eye viewports render cleanly with zero GL errors and visible stereo parallax
