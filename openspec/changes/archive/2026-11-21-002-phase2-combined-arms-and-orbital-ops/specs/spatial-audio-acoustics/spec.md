# Spec Delta

## Purpose
Delivers 3D spatial audio playback behind a hardware-abstracted backend supporting distance attenuation, interior hull acoustic muffling, orbital vacuum silence, and external WAV sound assets.

## ADDED Requirements

### Requirement: 3D Spatial Audio and Interior Acoustics
The audio engine SHALL compute 3D distance attenuation, stereo pan, and interior hull low-pass muffling when the listener occupies an armored interior frame.

#### Scenario: Interior hull muffles exterior plasma fire
- **GIVEN** a Commander standing inside an armored Citadel interior with muffle factor 0.65
- **WHEN** an exterior plasma sound plays 50 meters away
- **THEN** the emitted voice has reduced gain and active low-pass muffling compared to an exterior listener

### Requirement: Orbital Vacuum Silence with Radio Bypass
Exterior airborne sounds originating above the planetary atmospheric ceiling SHALL attenuate to silence in vacuum unless flagged as hull-conducted vibration or radio comms chatter.

#### Scenario: Vacuum silences exterior explosions but preserves radio comms
- **GIVEN** a listener outside a Space Station at 400km orbital altitude
- **WHEN** an exterior airborne explosion sound and an internal radio comms sound are triggered
- **THEN** the airborne explosion gain attenuates to zero while the radio comms voice plays at full clarity
