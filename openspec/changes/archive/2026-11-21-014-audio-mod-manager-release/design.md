# Design: Milestone 014 — External Audio Suite, Mod Manager & Desktop Release Packaging

## Architecture
1. **External CC BY 4.0 Audio Suite (`tools/generate_audio_suite.py` -> `assets/audio/*.wav`):**
   - Synthesizes 24 22050 Hz 16-bit mono PCM WAV SFX clips covering footsteps, airlocks, turbolifts, all 6 weapon families, shields, EMP, decompression, escape pod launch, 1:1 welding, explosions, radar/comms/UI cues, thrusters, naval splash, hangar doors, and victory fanfare.
   - Synthesizes 3 looping adaptive Solaris synth music stems (`music_solaris_orbit.wav`, `music_surface_command.wav`, `music_red_alert_battle.wav`) kept compact (~140 KB each) for fast GitHub Pages fetch.
   - `RtsAudioEngine` gains `rts_audio_select_music_track`, `rts_audio_update_music`, and `rts_audio_play_sfx_event`, maintaining vacuum silence for exterior airborne SFX and hull-conducted muffling for interior combat.
2. **In-Game Mod Manager & Live Hot-Reload (`include/rts/mod_manager.h`, `src/mod_manager.c`):**
   - Cross-platform directory scanner (`opendir`/`readdir` on POSIX/Emscripten MEMFS, `FindFirstFileA` on Win32) reads `<base_dir>/mods/*/mod.json`, extracting `id`, `name`, `version`, and `description`, sorted alphabetically by `id`.
   - `rts_vfs_unmount_mod` / `rts_vfs_clear_mods` cleanly rebuild or toggle active VFS mod overlays.
   - `rts_renderer_reload_textures` re-uploads dynamic hull/interior/icon textures through `RtsVfs` so toggling `example_mod` or `solaris_gold_fleet` updates 3D hulls live on screen without restarting.
3. **Desktop Release Workflow (`.github/workflows/release.yml`):**
   - Builds Release binaries on `ubuntu-latest`, `macos-latest`, and `windows-latest`, copies loose `assets/`, `scripts/`, `config/`, `mods/`, `manifest.json`, `README.md`, and licenses alongside the executable, runs the 28-scenario headless verification directly from the staged bundle, archives per-platform packages, and publishes them via `gh release create` on `v*` tags.
