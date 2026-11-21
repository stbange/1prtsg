# lua-gameplay-layer Specification

## Purpose
Embeds a sandboxed Lua 5.4 runtime that separates all game content, doubled unit/building blueprints, crew roles, AI logic, and match rules from the compiled C11 core engine.

## Requirements

### Requirement: Sandboxed Lua 5.4 Runtime
The engine SHALL execute gameplay scripts inside a sandboxed Lua 5.4 VM that removes direct host OS shell/file libraries and routes `require` exclusively through the VFS.

#### Scenario: Blocking unsafe host libraries
- **GIVEN** an initialized engine Lua VM
- **WHEN** a script inspects `os.execute`, `io`, or `package.loadlib`
- **THEN** those unsafe globals are nil while VFS-backed `require` loads external scripts cleanly

### Requirement: Doubled 76-Construct and 12-Role Catalog
External Lua scripts under `scripts/units/` and `scripts/crew/` SHALL define and register all 76 enterable constructs (24 Buildings, 10 Ground Vehicles, 10 Naval Ships, 8 Aircraft, 14 Spacecraft, 10 Space Stations) and 12 crew roles without hardcoding them in C.

#### Scenario: Validating full external roster registration
- **GIVEN** the base `scripts/` directory mounted in VFS
- **WHEN** `scripts/main.lua` initializes the unit and crew registries
- **THEN** exactly 76 unique enterable constructs across all 6 scale classes and 12 crew roles are registered with valid dimensions, interior rooms, and faction metadata

### Requirement: Multi-Theater Economy Sensors and AI Bindings
The Lua 5.4 runtime SHALL expose `engine.audio`, `engine.economy`, `engine.sensors`, and `engine.control` bindings supporting external Lua economy rules, sensor/relay checks, Astrogator flight planning, diegetic UI pages, and multi-theater Skirmish AI.

#### Scenario: Skirmish AI queues units and plans orbital operations
- **GIVEN** an initialized combined-arms skirmish mission with Solar Directorate and Crimson Dominion factions
- **WHEN** the Lua Skirmish AI ticks across ground, naval, air, and orbital theaters
- **THEN** it queues valid constructs via `engine.economy`, checks relay/sensor links, and dispatches tactical orders

### Requirement: Boarding and Reactor Meltdown Lua API
The Lua 5.4 runtime SHALL expose `engine.boarding` functions enabling external Lua scripts to start boarding assaults, breach/decompress decks, trigger reactor meltdowns, and step boarding/meltdown state deterministically.

#### Scenario: Lua capital assault mission captures enemy flagship
- **GIVEN** `scripts/missions/capital_assault.lua` and `scripts/rules/boarding.lua`
- **WHEN** Lua launches a boarding action and advances simulation ticks
- **THEN** the target flagship transitions cleanly to friendly ownership via `engine.boarding`
