# Spec Delta

## Purpose
Provides deterministic multi-deck boarding assaults, hull decompression vacuum hazards, intact capital vessel Bridge capture, and Critical Reactor Meltdown countdown evacuation mechanics.

## ADDED Requirements

### Requirement: Multi-Deck Capital Ship Boarding and Intact Capture
Marine squads SHALL be able to breach enemy capital ships, naval vessels, stations, and buildings deck by deck (`Hangar/Airlock` -> `Engineering` -> `Bridge/CIC`), optionally triggering hull decompression hazards, and capture the vessel intact by flipping its owning `faction_id` upon securing the Bridge.

#### Scenario: Capturing an enemy Dreadnought intact via Bridge breach
- **GIVEN** a Crimson Dominion Dreadnought with defenders across decks
- **WHEN** Solar Directorate Marines launch a boarding assault and advance through Hangar, Engineering, and Bridge decks
- **THEN** the boarding operation completes as `CAPTURED` and the Dreadnought's `faction_id` transfers intact to Solar Directorate

### Requirement: Critical Reactor Meltdown and Ironman Escape-Pod Sprint
A critically damaged or scuttled capital vessel SHALL support a timed Reactor Meltdown countdown that destroys the vessel upon reaching zero, causing Hardcore Ironman defeat if the Commander remains aboard or survival if the Commander ejects via an Escape Pod before detonation.

#### Scenario: Surviving a Reactor Meltdown via Escape Pod vs Ironman defeat
- **GIVEN** a Commander aboard a Dreadnought undergoing a 10-second Reactor Meltdown countdown
- **WHEN** the Commander launches an Escape Pod before `t = 0`
- **THEN** the Dreadnought detonates while the Commander remains alive in `EVACUATED_SAFE`; conversely, remaining aboard until `t = 0` triggers `DEFEAT_IRONMAN`
