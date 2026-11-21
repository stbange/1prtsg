# vfs-and-mods Specification

## Purpose
Provides an external, layered Virtual File System (VFS) so all game assets, shaders, configurations, and Lua scripts reside outside executables and support safe runtime mod overrides.

## Requirements

### Requirement: External Asset Storage
The engine MUST load all game assets, shaders, configuration files, and Lua scripts from external sibling directories (`assets/`, `scripts/`, `mods/`, `config/`) without baking game content into executables or WASM binaries.

#### Scenario: Resolving base external assets
- **GIVEN** an application root containing `assets/` and `scripts/`
- **WHEN** the engine requests `textures/hull_blue.png` or `main.lua`
- **THEN** the VFS loads the bytes directly from the external directory on disk or relative HTTP root

### Requirement: Layered Mod Precedence
The VFS SHALL layer enabled mods in configured load order on top of base `assets/` and `scripts/` directories so later mods override earlier mods and base files.

#### Scenario: Mod overrides base unit script and texture
- **GIVEN** an enabled mod in `mods/example_mod/` containing `scripts/units/ground_vehicles.lua`
- **WHEN** the engine resolves `units/ground_vehicles.lua`
- **THEN** the VFS returns the file from `mods/example_mod/` instead of the base `scripts/` directory

### Requirement: Path Traversal Sandbox
The VFS MUST reject any logical path containing `..`, leading directory separators, Windows drive prefixes, or embedded null bytes.

#### Scenario: Rejecting directory escape attempts
- **GIVEN** a mounted VFS root
- **WHEN** a caller or Lua script requests `../secret.txt` or `/etc/passwd`
- **THEN** the VFS rejects the request with an invalid-path error without touching the host filesystem
