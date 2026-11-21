# Spec Delta

## ADDED Requirements

### Requirement: Multi-Theater Economy Sensors and AI Bindings
The Lua 5.4 runtime SHALL expose `engine.audio`, `engine.economy`, `engine.sensors`, and `engine.control` bindings supporting external Lua economy rules, sensor/relay checks, Astrogator flight planning, diegetic UI pages, and multi-theater Skirmish AI.

#### Scenario: Skirmish AI queues units and plans orbital operations
- **GIVEN** an initialized combined-arms skirmish mission with Solar Directorate and Crimson Dominion factions
- **WHEN** the Lua Skirmish AI ticks across ground, naval, air, and orbital theaters
- **THEN** it queues valid constructs via `engine.economy`, checks relay/sensor links, and dispatches tactical orders
