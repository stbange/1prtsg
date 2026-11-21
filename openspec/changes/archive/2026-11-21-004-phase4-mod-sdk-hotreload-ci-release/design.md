# Design: Phase 4 — Mod SDK, Hot-Reload, Save/Load & CI

## Architecture
- **External Config (`RtsEngineConfig`):** Key-value parser ignoring `#` comments and blank lines; populates defaults cleanly so missing keys fall back safely. Applies `hardcore_ironman` and `planet_radius_m` directly to `RtsWorld` and mounts any comma-separated `enabled_mods` via `rts_vfs_mount_mod`.
- **VFS Hot-Reload Watcher (`RtsHotReloadWatch`):** Stores up to `RTS_VFS_MAX_WATCHES` entries `(category, logical_path, last_mtime)`. `rts_vfs_check_hot_reload` re-queries `rts_vfs_mtime` through `RtsFsBackend` (honoring mod overlays!) and returns changed watch indices so callers can reload Lua modules or textures live.
- **Deterministic Save/Load (`rts_save_serialize` / `rts_save_deserialize`):** Serializes an ASCII `RTS_SAVE_V1` header + `WORLD`, `CMD`, `LEDGER`, and `ENT` records using full 64-bit `%.17g` floating-point precision so re-hydrating into a fresh `RtsWorld` + `RtsEconomySystem` produces identical future simulation states.
- **Mod Override Visibility:** When `vfs->mod_count > 0`, `src/retro_renderer.c` displays the active mod badge in the top HUD alongside overridden external textures (e.g., `mods/example_mod/assets/textures/hull_blue.png` Emerald Vanguard armor).
- **CI Matrix (`.github/workflows/ci.yml`):**
  - `linux-coverage-and-gl`: GCC + Ninja + CTest + `tools/check_coverage.py --min-line 95.0` + Lua coverage >= 95% + Xvfb Mesa OpenGL screenshot smoke test (`gl_errors == 0`).
  - `linux-sanitizers`: GCC/Clang AddressSanitizer + UndefinedBehaviorSanitizer build & CTest.
  - `windows-msvc`: Windows MSVC CMake build & CTest.
  - `macos-clang`: macOS Clang CMake build & CTest.
  - `web-emscripten`: Emscripten SDK (`emcmake`) build verifying zero baked assets and valid `.wasm`/`.js` + `manifest.json` output.
