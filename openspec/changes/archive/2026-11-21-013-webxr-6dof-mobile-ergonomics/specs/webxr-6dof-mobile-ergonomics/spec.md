# Spec: True 6-DOF WebXR Session Loop & Browser / Mobile Ergonomics

## ADDED Requirements

### Requirement: Per-Frame 6-DOF WebXR Viewer Pose & Controller Streaming
The WebXR bridge SHALL run `xrSession.requestAnimationFrame` streaming per-frame `XRViewerPose` head pose, left/right 4x4 eye view and projection matrices, left-hand 6-DOF Tactical Wrist-Pad pose, right-hand aim ray and trigger state, thumbsticks, and VR comfort settings (`comfort_vignette`, `seated_cockpit_mode`, `snap_turn_deg`) into C via `rts_webxr_submit_pose`.

### Requirement: 2-Finger Mobile Pinch-Zoom/Rotate Holo-Globe & Responsive Browser HUD
The input router SHALL recognize 2-finger touch pinch-zoom and rotate gestures driving 3D Holo-Globe zoom and orbital rotation smoothly on mobile devices alongside a responsive portrait/landscape WebGL 2 toolbar with Menu, Pointer Lock, WebAudio unlock, and Touch HUD controls.
