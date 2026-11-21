# Tasks

## 1. Config Loader, VFS Hot-Reload & Deterministic Save/Load (`rts_core`)
- [x] 1.1 Implement `include/rts/config.h` and `src/config.c` parsing external `config/engine.cfg` via VFS
- [x] 1.2 Extend `include/rts/vfs.h` and `src/vfs.c` with `RtsHotReloadWatch`, `rts_vfs_watch_add`, and `rts_vfs_check_hot_reload`
- [x] 1.3 Implement `include/rts/save_load.h` and `src/save_load.c` for deterministic `RTS_SAVE_V1` world & economy snapshot round-trips
- [x] 1.4 Add `tests/test_config_and_save.c` verifying config parsing, VFS hot-reload change detection, and bit-for-bit deterministic save/load round-trips

## 2. Lua Session Bindings, Mod Showcase & Modding SDK Docs
- [x] 2.1 Expose `engine.config`, `engine.vfs.watch`/`poll_hot_reload`, and `engine.save.snapshot`/`restore` in `src/lua_vm.c` and create `scripts/rules/session.lua`
- [x] 2.2 Show active mod badge in `src/retro_renderer.c`, capture `docs/screenshots/10_mod_emerald_vanguard.png` with `--mod example_mod`, and visually inspect it
- [x] 2.3 Write `docs/MODDING_GUIDE.md`, `.github/workflows/ci.yml`, and update `README.md` with full Phase 1–4 documentation and screenshot gallery

## 3. Final Release Verification & Archive
- [x] 3.1 Expand Lua unit tests and automated game tests (Scenarios 16–18), verify >= 95% C11 & Lua coverage, zero GL errors across all 10 screenshots, and archive Phase 4 OpenSpec change
