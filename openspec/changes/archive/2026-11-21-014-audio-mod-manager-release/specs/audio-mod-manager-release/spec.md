# Specification: External Audio Suite, Mod Manager & Desktop Release Packaging

## Requirements

### Requirement: External CC BY 4.0 Audio Suite & Adaptive Music
The game SHALL ship at least 24 loose external PCM WAV sound effects and 3 adaptive Solaris music stems under `assets/audio/`, indexed in `manifest.json` with zero baked assets.

#### Scenario: All 27 external WAV files decode cleanly through VFS
- **WHEN** `RtsAudioEngine` loads every SFX and music stem through `RtsVfs`
- **THEN** all 27 files decode as valid 16-bit PCM WAV data, exterior airborne SFX are silenced in orbital vacuum, interior/hull channels apply low-pass muffling, and radio/music channels remain clear.

### Requirement: In-Game Mod Browser & Zero-Restart Hot-Reload
The game SHALL dynamically scan `mods/*/mod.json`, list all discovered mods inside the Solaris Mod Manager UI, allow enabling/disabling individual mods, and hot-reload Lua scripts and overridden renderer textures at runtime without restarting.

#### Scenario: Live mod toggle and texture hot-reload
- **WHEN** the player toggles `example_mod` ("Emerald Vanguard Overhaul") or `solaris_gold_fleet` ("Solaris Gold Fleet") in the Mod Manager and activates Hot-Reload
- **THEN** `RtsVfs` updates active overlays, `RtsLuaVm` re-executes `main.lua`, and `RtsRetroRenderer` re-uploads overridden external textures live.

### Requirement: Cross-Platform Loose-Asset Desktop Release Workflow
The repository SHALL include `.github/workflows/release.yml` that builds, smoke-tests, packages, and publishes loose-asset `linux-x64`, `macos-arm64`, and `windows-x64` archives automatically on `v*` Git tags.
