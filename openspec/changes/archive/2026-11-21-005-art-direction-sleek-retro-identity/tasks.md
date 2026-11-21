# Tasks

## 1. Engine & Shader Visual Pipeline Upgrade
- [x] 1.1 Extend `src/obj_loader.c` to parse optional OBJ `v x y z r g b` vertex colors (preserving 3-float fallback) and test in `tests/test_assets_and_renderer.c`
- [x] 1.2 Add `cam_pos` & `fog_color` uniforms to `RtsDrawCmd` (`include/rts/gfx.h`, `src/gfx_gl.c`) and add `sh_sky`, `mesh_arch`, `mesh_terrain` to `RtsRetroRenderer`
- [x] 1.3 Upgrade external GLSL shaders (`mesh.vert`, `mesh.frag`, `shield.frag`, `holo.frag`) and create `sky.vert` & `sky.frag`

## 2. Sculpted External Meshes & Solaris Palette Art Assets
- [x] 2.1 Rebuild `tools/generate_retro_assets.py` with parametric loft/wing/dome/ring sculptors for all 10 `.obj` meshes (`citadel`, `dropship`, `plasma_tank`, `dreadnought`, `shipyard`, `starfighter`, `carrier`, `crew_3d`, `interior_arch`, `terrain_island`)
- [x] 2.2 Upgrade external PNG textures (256px brushed-titanium/obsidian/emerald hulls, 512px Helios Prime surface, caustic ocean, sleek CRT console, 256×64 8-dir crew sprites) and regenerate `manifest.json`

## 3. Scene Composition, Verification & Visual Inspection
- [x] 3.1 Upgrade `src/retro_renderer.c` scenes with twilight/nebula sky domes, vaulted arch ribs, sculpted terrain, glowing thruster plumes, and refined frosted-glass HUD framing
- [x] 3.2 Run full verification (C/Lua unit tests, >= 95% C & Lua coverage, 18 game scenarios, ASan/UBSan 0 leaks, 10 live OpenGL screenshots with 0 GL errors), visually inspect ALL 10 screenshots via `view_file`, archive OpenSpec change, and push to `main`
