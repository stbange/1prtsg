# Proposal: Phase 4 — Mod SDK, External Config, VFS Hot-Reload, Save/Load & Cross-Platform CI

## Why
To fulfill 1stPRTS's promise as an extendable, cross-platform, strictly spec-driven open-source engine (MIT code + CC BY 4.0 art), players and modders need external configuration loading (`config/engine.cfg`), live VFS hot-reload detection for rapid Lua/asset iteration, deterministic Save/Load snapshots, thorough modding documentation, and automated cross-platform CI (Linux, Windows, macOS, Emscripten Web/WebXR) enforcing the >= 95% unit test coverage gate.

## What Changes
- **External Config Loader (`include/rts/config.h`, `src/config.c`):** Parses `config/engine.cfg` via VFS (resolution, nearest-neighbor toggle, Ironman default, crew sprite LOD threshold, planet radius, and `enabled_mods` auto-mount list).
- **VFS Live Hot-Reload Watcher (`include/rts/vfs.h`, `src/vfs.c`):** Tracks file mtimes across mounted mods and base folders so modified Lua scripts or assets are detected at runtime without restarting.
- **Deterministic Save/Load Serializer (`include/rts/save_load.h`, `src/save_load.c`):** Portable text snapshot serialization/deserialization preserving world tick, RNG state, commander state, faction ledgers, and all active constructs bit-for-bit deterministically.
- **Lua Session API & Modding Guide (`src/lua_vm.c`, `scripts/rules/session.lua`, `docs/MODDING_GUIDE.md`):** Exposes `engine.config`, `engine.vfs.watch`/`poll_hot_reload`, and `engine.save.snapshot`/`restore` to Lua, documents the full Mod SDK, and showcases live mod override rendering (`10_mod_emerald_vanguard.png`).
- **Cross-Platform CI & Final Release Polish (`.github/workflows/ci.yml`, `README.md`):** Multi-OS GitHub Actions workflow + final full test, >= 95% C & Lua coverage, and visual screenshot verification.
