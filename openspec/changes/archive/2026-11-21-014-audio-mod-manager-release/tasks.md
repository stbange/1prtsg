# Tasks

## 1. Full External CC BY 4.0 Audio Suite & Adaptive Acoustics
- [x] 1.1 Create `tools/generate_audio_suite.py` generating 24 external WAV SFX clips + 3 adaptive Solaris music stems under `assets/audio/` and run it
- [x] 1.2 Extend `include/rts/audio.h` & `src/audio.c` with `RtsMusicState`, `rts_audio_select_music_track`, `rts_audio_update_music`, and `rts_audio_play_sfx_event`, wired into `src/platform_sdl.c`

## 2. In-Game Mod Browser, Second Showcase Mod & Live Hot-Reload
- [x] 2.1 Add `rts_vfs_unmount_mod` / `rts_vfs_clear_mods` in `include/rts/vfs.h` & `src/vfs.c` and `rts_renderer_reload_textures` in `include/rts/gfx.h` & `src/retro_renderer.c`
- [x] 2.2 Create `include/rts/mod_manager.h`, `src/mod_manager.c`, and second loose mod `mods/solaris_gold_fleet/`
- [x] 2.3 Upgrade `RTS_MENU_MODS` in `include/rts/ui_menus.h`, `src/ui_menus.c`, `src/retro_renderer.c`, and `src/platform_sdl.c` for live multi-mod browsing, toggling, and zero-restart hot-reload

## 3. Desktop Release Packaging, Verification, Fun Audit & `v1.0.0` Tag
- [x] 3.1 Create `.github/workflows/release.yml` packaging loose-asset Windows/macOS/Linux bundles on `v*` tags
- [x] 3.2 Add `tests/test_audio_mods.c`, Scenario 28, `--capture-milestone-014`, visually inspect `m014_01..02_*.png`, run `--fun-eval`, and verify >=95% C11 & Lua coverage + 0 ASan leaks
- [x] 3.3 Archive OpenSpec `014`, update `README.md` & `manifest.json`, push to `main`, verify 5/5 CI + Pages green, then tag & push `v1.0.0` and verify Release workflow green
