# Spec Delta

## Purpose
Provides a unified input intent router that adapts seamlessly across Desktop Keyboard/Mouse, Gamepad, automatic Mobile Web Touch controls, and 6-DOF WebXR sessions.

## ADDED Requirements

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
