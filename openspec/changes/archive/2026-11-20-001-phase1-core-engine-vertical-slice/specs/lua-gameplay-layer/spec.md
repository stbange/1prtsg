# Spec Delta

## Purpose
Embeds a sandboxed Lua 5.4 runtime that separates all game content, doubled unit/building blueprints, crew roles, AI logic, and match rules from the compiled C11 core engine.

## ADDED Requirements

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
