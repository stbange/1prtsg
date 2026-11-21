# mod-sdk-and-hotreload Specification

## Purpose
Provides external engine configuration loading, live VFS hot-reload change detection for mods and scripts, deterministic save/load world snapshots, and complete Mod SDK documentation.

## Requirements

### Requirement: External Configuration Loading
The engine SHALL parse external `config/engine.cfg` files via VFS without recompilation, supporting resolution, retro texture filtering, Hardcore Ironman mode, crew LOD threshold, planetary radius, and an ordered `enabled_mods` list.

#### Scenario: Loading config/engine.cfg and applying world settings
- **GIVEN** an external `config/engine.cfg` specifying `hardcore_ironman = true` and `window_width = 1280`
- **WHEN** `rts_config_load_vfs` parses the file and applies it to `RtsWorld`
- **THEN** all parsed fields match the file values and invalid/missing optional keys fall back safely to defaults

### Requirement: Live VFS Hot-Reload Detection
The VFS SHALL support registering watched external assets and Lua scripts and polling mtimes through the active mod overlay stack to detect modifications at runtime.

#### Scenario: Detecting modified Lua script during live session
- **GIVEN** a watched script registered in `RtsHotReloadWatch`
- **WHEN** the script's mtime increases on disk or via a mounted mod overlay
- **THEN** `rts_vfs_check_hot_reload` reports the changed watch entry on the next poll and clears the dirty state afterward

### Requirement: Deterministic World and Economy Save/Load Snapshots
The engine SHALL serialize and deserialize world ticks, RNG state, commander state, economy ledgers, and active entities into a portable text snapshot such that continuing simulation after reload matches an uninterrupted run deterministically.

#### Scenario: Save/load round-trip preserves deterministic simulation trajectory
- **GIVEN** an active world with spawned units, faction ledgers, and a moving tank
- **WHEN** a snapshot is serialized, restored into a fresh `RtsWorld` + `RtsEconomySystem`, and stepped forward
- **THEN** entity coordinates, hull/shield values, RNG state, and ledgers match the original uninterrupted run
