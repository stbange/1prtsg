<!-- SPDX-License-Identifier: MIT -->
# 1stPRTS (`1prtsg`) Modding Guide & SDK

Every single construct blueprint, crew role, AI script, combat rule, texture, 3D model, voxel fortification, shader, sound effect, and configuration option in **1stPRTS** lives in plain sibling folders beside the `1prtsg` executable — **zero game content is baked into the compiled C11 binary**.

---

## 1. Directory Layout Beside the Binary

```text
1prtsg/
├── 1prtsg(.exe/.wasm)          # Pure C11 + SDL3 + OpenGL core engine
├── config/
│   └── engine.cfg              # Editable resolution, Ironman rules, LOD, enabled_mods
├── assets/                     # CC BY 4.0 art, shaders, meshes, voxels, audio
│   ├── shaders/                # External GLSL (.vert/.frag) shaders
│   ├── textures/               # Nearest-neighbor retro PNG textures
│   ├── sprites/                # 8-/16-directional crew pixel spritesheets
│   ├── fonts/                  # Retro bitmap font atlas
│   ├── models/                 # Low-poly Wavefront .obj constructs & crew
│   ├── voxels/                 # Optional .vxl fortification chunks
│   ├── audio/                  # Uncompressed PCM .wav SFX & radio alerts
│   └── planets/                # Spherical planet parameters (JSON)
├── scripts/                    # MIT-licensed editable Lua 5.4 gameplay layer
│   ├── main.lua                # Entry point registering all 76 constructs & 12 roles
│   ├── units/                  # 6 scale-class catalogs + registry.lua
│   ├── crew/                   # 12 crew role definitions
│   ├── factions/               # Faction liveries & metadata
│   ├── rules/                  # Combat, Ironman, Economy, Sensors, Boarding, Session
│   ├── ai/                     # Commander AI, Astrogator autopilot, Skirmish AI
│   ├── ui/                     # Diegetic Tactical Wrist-Pad & CRT consoles
│   └── missions/               # Vertical Slice, Combined Arms, Capital Assault
└── mods/
    └── example_mod/            # Sample Emerald Vanguard mod (texture + Lua override)
        ├── mod.json
        ├── assets/textures/hull_blue.png
        └── scripts/units/ground_vehicles.lua
```

---

## 2. Creating Your First Mod

1. Create a folder under `mods/<your_mod_id>/` with a `mod.json` manifest:
   ```json
   {
     "id": "my_custom_fleet",
     "name": "My Custom Fleet Pack",
     "version": "1.0.0",
     "author": "Your Name",
     "description": "Custom capital ship liveries and balanced plasma weapons."
   }
   ```
2. Mirror any relative path from `assets/` or `scripts/` inside your mod folder:
   - Override textures: `mods/my_custom_fleet/assets/textures/hull_blue.png`
   - Override 3D models: `mods/my_custom_fleet/assets/models/dreadnought.obj`
   - Override Lua unit stats or add new constructs: `mods/my_custom_fleet/scripts/units/spacecraft.lua`
   - Change Hardcore Ironman match rules: `mods/my_custom_fleet/scripts/rules/ironman.lua`
3. Activate your mod either:
   - Via CLI flag: `./1prtsg --data-dir . --mod my_custom_fleet`
   - Or persistently in `config/engine.cfg`:
     ```ini
     enabled_mods = example_mod, my_custom_fleet
     ```
   Later mods in the list override earlier mods, and all mods override base `assets/` and `scripts/`.

---

## 3. Lua 5.4 Engine API Reference (`engine.*`)

The C11 core exposes a sandboxed Lua 5.4 runtime (`io`, `dofile`, `loadfile`, `os.execute`, and `package.loadlib` are removed; `require()` resolves exclusively through the VFS):

| Namespace | Functions |
|---|---|
| `engine.vfs` | `exists(cat, path)`, `read_text(cat, path)`, `mount_mod(id)`, `watch(cat, path)`, `poll_hot_reload()` |
| `engine.config` | `load(path)` |
| `engine.save` | `snapshot()`, `restore(snapshot_text)` |
| `engine.planet` | `altitude(x, y, z)`, `atmo_density(alt_m)` |
| `engine.orbit` | `circular_speed(alt_m)`, `hohmann_dv(alt1_m, alt2_m)` |
| `engine.world` | `register_blueprint(tbl)`, `register_crew_role(tbl)`, `counts()`, `spawn(...)`, `get_entity(id)`, `set_orbital(...)`, `issue_order(...)`, `apply_damage(...)`, `step(dt, n)`, `commander_state()`, `commander_board(id)`, `commander_escape_pod()`, `eval_crew_lod(...)` |
| `engine.interior` | `add_portal_and_transfer(frame_a, frame_b)` |
| `engine.economy` | `queue_build(producer_id, bp_id)`, `step(dt, n)`, `job_spawned(job_id)`, `ledger(faction_id)` |
| `engine.sensors` | `is_detected(faction_id, target_id)`, `is_comms_linked(hq_id, unit_id)` |
| `engine.control` | `engage_pilot(craft_id)`, `release_pilot()`, `astrogator_ascent(id, alt)`, `astrogator_reentry(id)`, `astrogator_step(dt, n)` |
| `engine.boarding` | `start(target_id, fac, marines, mag_boots)`, `set_decompression(op_id, bool)`, `get_op(op_id)`, `trigger_meltdown(id, sec)`, `get_meltdown(id)`, `step(dt, n)` |
| `engine.audio` | `set_listener(x, y, z, alt_m, muffle)`, `play_3d(wav, x, y, z, channel, vol, max_dist)` |

---

## 4. Web / WebXR Deployment & `manifest.json`

When deploying mods or edited assets to the Emscripten Web/WebXR build, regenerate `manifest.json` so `web/index.html` fetches all loose external files cleanly over HTTP (zero baked bundles):

```bash
python3 tools/generate_manifest.py
```

---

## 5. Dual Licensing for Contributors & Modders

- **Code, Lua Scripts, Shaders, CMake & Tests:** MIT License (`LICENSE-CODE`)
- **Art, Textures, 3D Models, Voxels & Audio:** Creative Commons Attribution 4.0 International (`LICENSE-ART`)
