# 1stPRTS (`1prtsg`)

> **Command an empire from the boots up.**  
> An exclusively first-person Real-Time Strategy (RTS) game where every building, vehicle, naval ship, aircraft, spacecraft, and space station is realistically huge, manned by simulated crew, and fully enterable and explorable.

---

## Core Vision

Traditional RTS games put you in the sky as an invisible, omniscient camera. **1stPRTS** puts you physically **inside the world as the Commander**:

- **Exclusively First-Person Perspective:** No disembodied god-camera. You direct armies, fleets, and orbital shipyards through diegetic, physical tools — your tactical wrist-pad, optical laser designator, vehicle/ship bridges, air-traffic towers, and massive 3D holographic War Room tables.
- **Realistically Huge, Manned & Enterable Everything:** Every structure and vessel across all six scale classes — **buildings, ground vehicles, naval ships, aircraft, spacecraft, and space stations** — is built to 1:1 human scale, staffed by active crew with physical roles, and can be walked through, operated, repaired, or boarded.
- **Retro 3D Aesthetic:** Crisp, fast late-90s / early-2000s tactical sci-fi visuals combining **low-poly 3D meshes**, **multi-directional 2.5D/3D sprites**, and **optional chunked voxels**, complete with pixel-filtered textures and diegetic CRT consoles.
- **Engine / Script Split & True Moddability:**
  - **Core Engine (`C11` + `SDL3` + `OpenGL`):** High-performance rendering, 64-bit multi-scale interior/exterior spatial physics, deterministic simulation, audio, input, and Virtual File System (VFS).
  - **Gameplay Layer (`Lua 5.4`):** Units, factions, crew AI, tactical AI, economy, missions, and diegetic UI live in editable `.lua` scripts outside the executable.
  - **Zero Baked Assets:** All graphics, sounds, shaders, interiors, maps, and scripts sit in folders beside the binary (`assets/`, `scripts/`, `mods/`) and support layered mod overrides and hot-reloading.

---

## Target Platforms

| Platform | Graphics / Runtime | Input Modes |
| :--- | :--- | :--- |
| **Windows** (x64) | SDL3 + OpenGL 3.3 Core | Keyboard + Mouse, Gamepad |
| **Linux** (x64 / arm64) | SDL3 + OpenGL 3.3 Core / GLES 3.0 | Keyboard + Mouse, Gamepad |
| **macOS** (Apple Silicon / Intel) | SDL3 + OpenGL 3.3 / 4.1 Core | Keyboard + Mouse, Gamepad |
| **Web** (Emscripten / WASM) | SDL3 + WebGL 2.0 (loose external HTTP assets) | Keyboard + Mouse, Gamepad, **Auto-Touch on Mobile** |
| **WebXR** (VR/XR via Web build) | Stereo WebGL 2.0 via WebXR Session | 6-DOF Head Tracking + Hand/Controller Rays |

---

## Engineering & Quality Standards

This project is built from day one around strict specification and verification discipline:

1. **OpenSpec Spec-Driven Development:** Every capability is proposed, specified (`proposal.md`, `specs/<capability>/spec.md`, `design.md`, `tasks.md`), implemented, verified, and archived under [`openspec/`](openspec/config.yaml).
2. **Unit Test Coverage >= 95%:** Both the C core engine (`gcov`/`llvm-cov` via CTest) and Lua script layer (VM line-coverage hook) enforce a minimum **95% unit test coverage** gate using strict hardware-abstraction seams.
3. **Automated Game Tests:** Headless deterministic end-to-end Lua game scenarios exercise full commander workflows (walking interiors, issuing holo-table orders, boarding dropships, transitioning to orbital stations, mobile touch input, WebXR ray interaction, and mod overrides), backed by offscreen OpenGL and headless browser WebGL2 smoke tests.

---

## Documentation

| Document | Purpose |
| :--- | :--- |
| [`docs/CONCEPT.md`](docs/CONCEPT.md) | **Game Concept & Design Bible** — first-person command toolkit, all 6 enterable scale classes, living crew & subsystem damage, multi-theater escalation, retro 3D art/audio direction, controls, modding, phased roadmap, and open design decisions. |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | **Technical Architecture Blueprint** — C/Lua boundary, 64-bit world & nested interior reference frames, external VFS (desktop & Web), unified OpenGL/WebGL2/WebXR pipeline, input switching, and >= 95% test/coverage + automated game test design. |
| [`openspec/config.yaml`](openspec/config.yaml) | **OpenSpec Configuration** — spec-driven workflow configuration, project context, and verification rules. |

---

## Planned Repository & Runtime Layout

```text
1prtsg/
├── LICENSE                  # Dual-license overview
├── LICENSE-CODE             # MIT License (code, scripts, shaders, tests, docs)
├── LICENSE-ART              # CC BY 4.0 License (art and audio assets)
├── README.md
├── docs/                    # Concept bible and architecture blueprints
├── openspec/                # OpenSpec config, canonical specs/, and changes/
├── include/                 # Public C11 core engine headers (planned)
├── src/                     # C11 core engine implementation (planned)
├── tests/                   # C unit tests, Lua unit tests, and automated game tests (planned)
├── assets/                  # External CC BY 4.0 assets (models, textures, sprites, voxels, audio, maps) + MIT shaders
├── scripts/                 # External MIT Lua 5.4 gameplay scripts (factions, units, crew, AI, UI, missions)
└── mods/                    # External user mod packages layered over assets/ and scripts/
```

---

## Project Status

**Current Stage:** Concept & Architecture Exploration (`Phase 0`).  
See the **Open Design Decisions** section in [`docs/CONCEPT.md`](docs/CONCEPT.md#10-open-design-decisions-for-review) before we draft the first OpenSpec change proposal for Phase 1.

---

## Licensing

- **Code, Lua Scripts, Shaders, Tests, and Documentation:** [MIT License](LICENSE-CODE)
- **Art and Audio Assets:** [Creative Commons Attribution 4.0 International (CC BY 4.0)](LICENSE-ART)

See [`LICENSE`](LICENSE) for exact scope boundaries.
