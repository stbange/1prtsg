# Proposal: Milestone 014 — External Audio Suite, Mod Manager & Desktop Release Packaging

## Why
1stPRTS now delivers the full planetary-to-orbital gameplay loop, interactive RTS command toolkit, 6-class vehicle physics, unified combat, strategic AI, 4 curated scenarios, Solaris menu system, and 6-DOF WebXR + mobile ergonomics. To ship `v1.0.0`, players need a rich external CC BY 4.0 soundscape (24+ SFX + 3 adaptive Solaris synth music stems), an in-game Mod Browser capable of discovering, toggling, and live hot-reloading loose external mods (scripts AND textures without restarting), and automated GitHub Releases packaging loose-asset desktop bundles for Windows, macOS, and Linux.

## What Changes
- **External CC BY 4.0 Audio Suite**: Generate 24 distinct retro-futurist WAV SFX clips and 3 adaptive Solaris space-opera synth music loops under `assets/audio/`, indexed in `manifest.json` (zero baked assets), with adaptive music state management and acoustic vacuum/interior filtering in `src/audio.c`.
- **In-Game Mod Manager & Live Hot-Reload**: Add `include/rts/mod_manager.h`, `src/mod_manager.c`, `rts_vfs_unmount_mod` / `rts_vfs_clear_mods` in `src/vfs.c`, `rts_renderer_reload_textures` in `src/retro_renderer.c`, a second loose showcase mod (`mods/solaris_gold_fleet/`), and dynamic mod browsing/toggling/hot-reload inside `RTS_MENU_MODS`.
- **Desktop Release Workflow**: Add `.github/workflows/release.yml` building, smoke-testing, and attaching `1prtsg-linux-x64.tar.gz`, `1prtsg-macos-arm64.tar.gz`, and `1prtsg-windows-x64.zip` (binary + loose external folders) to tagged GitHub Releases (`v*`).
- **Verification & `v1.0.0` Ship**: Add `tests/test_audio_mods.c`, Scenario 28, `--capture-milestone-014`, run final `--fun-eval` audit, confirm >=95% C11 & Lua coverage + 0 ASan leaks + 5/5 CI + Pages green, and publish `v1.0.0`.
