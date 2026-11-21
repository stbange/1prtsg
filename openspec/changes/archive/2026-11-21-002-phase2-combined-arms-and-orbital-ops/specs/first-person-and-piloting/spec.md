# Spec Delta

## Purpose
Enables live first-person Commander locomotion, physical console interaction, direct manual piloting override across all vehicle/craft classes, and automated Astrogator orbital ascent, transfer, docking, and shielded re-entry.

## ADDED Requirements

### Requirement: First-Person Locomotion and Console Use
The controller SHALL move the Commander inside occupied interior rooms or across planetary ENU surfaces using unified input intents, clamp movement to pressurized room bounds, and allow interacting with nearby consoles (Holo-Globe, Factory Terminal, Pilot Seat, Helm, Escape Pod).

#### Scenario: Walking inside Citadel and interacting with Holo-Globe
- **GIVEN** a Commander standing inside a Command Citadel War Room near a Holo-Globe console
- **WHEN** movement intents advance the Commander within interaction radius and interact is pressed
- **THEN** the Commander enters Holo-Globe console mode without leaving first-person perspective

### Requirement: Direct Piloting and AI Crew Handoff
The Commander SHALL be able to take direct manual control of any occupied vehicle, naval ship, aircraft, or spacecraft from a Pilot/Helm seat and hand control seamlessly back to AI crew upon standing up.

#### Scenario: Piloting a Dropship manually then returning control to AI
- **GIVEN** a Commander seated in a Dropship cockpit
- **WHEN** manual throttle and pitch/yaw intents are applied and the Commander later disengages the seat
- **THEN** the Dropship accelerates according to manual inputs and resumes autonomous orders once released

### Requirement: Automated Astrogator Orbital Flight
The Astrogator autopilot SHALL execute gravity-turn ascent into stable circular orbit, rendezvous/docking approach to an orbital station, and shielded atmospheric re-entry with thermal load absorbed by shields.

#### Scenario: Dropship ascent to 400km orbit and shielded re-entry
- **GIVEN** a Dropship capable of orbital flight
- **WHEN** an orbital ascent maneuver is executed followed by a de-orbit re-entry step through the upper atmosphere
- **THEN** the craft reaches stable vacuum orbit and later experiences measurable atmospheric drag and shield-absorbed re-entry heating on descent
