# 1stPRTS (`1prtsg`)

> **Command a planetary-to-orbital empire from the boots up — with one life on the line.**  
> An exclusively first-person Real-Time Strategy (RTS) game set in a bold-color retro space-opera universe on **true-scale spherical planets with real orbital mechanics**, where every building, vehicle, naval ship, aircraft, spacecraft, and space station is realistically huge, manned by simulated crew, and fully enterable and explorable.

---

## Core Vision

Traditional RTS games put you in the sky as an invisible, omniscient camera. **1stPRTS** puts you physically **inside the world as the Commander**:

- **Exclusively First-Person Perspective (Hardcore Ironman):** No disembodied god-camera — and if the Commander falls, the match is lost. Direct armies, fleets, and orbital shipyards through diegetic physical tools: your Tactical Wrist-Pad, Optical Laser Designator, vehicle/ship bridges, ATC towers, and room-sized **3D Planetary & Orbital Holo-Globes**.
- **True-Scale Spherical Worlds & Orbital Mechanics:** Seamless 64-bit spherical planets with continents, oceans, atmospheric drag, shielded re-entry, and authentic Keplerian orbits for satellites, spacecraft, and orbital shipyards.
- **Realistically Huge, Manned & Enterable Everything:** Every construct across all six scale classes — **buildings, ground vehicles, naval ships, aircraft, spacecraft, and space stations** — is built to 1:1 human scale, staffed by active crew with physical roles, and can be entered, operated, repaired, boarded, or evacuated via physical escape pods.
- **Classic Bold-Color Retro Space-Opera 3D:** Bold faction liveries, shimmering energy shield domes, plasma/beam/torpedo fire, sleek angular starfighters, **hybrid crew visuals** (low-poly 3D up close inside interiors & WebXR; 8-/16-directional pixel sprites at range), and **optional chunked voxels**.
- **Engine / Script Split & True Moddability:**
  - **Core Engine (`C11` + `SDL3` + `OpenGL`):** 64-bit cube-sphere & orbital physics, nested local interior frames, retro 3D/WebXR rendering, deterministic single-player simulation core, spatial audio, input, and layered VFS.
  - **Gameplay Layer (`Lua 5.4`):** Factions, units, crew AI, astrogator/commander AI, economy, Hardcore Ironman match rules, missions, and diegetic UI live in editable `.lua` scripts outside the executable.
  - **Zero Baked Assets:** All graphics, sounds, shaders, interiors, planet configs, and scripts sit in plain folders beside the binary (`assets/`, `scripts/`, `mods/`) with hot-reload support.

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

1. **OpenSpec Spec-Driven Development:** Every capability is proposed, specified (`proposal.md`, `specs/<capability>/spec.md`, `design.md`, `tasks.md`), implemented, verified, and archived under [`openspec/`](openspec/config.yaml).
2. **Unit Test Coverage >= 95%:** Both the C core engine (`gcov`/`llvm-cov` via CTest) and Lua script layer (VM line-coverage hook) enforce a minimum **95% unit test coverage** gate using strict hardware-abstraction seams.
3. **Automated Game Tests:** Headless deterministic end-to-end Lua game scenarios exercise full commander workflows (3D Holo-Globe orders, orbital ascent & station docking, Hardcore Ironman loss vs. escape-pod survival, hybrid crew LOD, localized damage/repair, mobile touch input, WebXR ray interaction, and mod overrides), backed by offscreen OpenGL and headless browser WebGL2 smoke tests.

---

## Documentation

| Document | Purpose |
| :--- | :--- |
| [`docs/CONCEPT.md`](docs/CONCEPT.md) | **Game Concept & Design Bible** — retro space-opera identity, 3-tier 1st-person command toolkit, 3D Planetary Holo-Globe, all 6 enterable scale classes, Hardcore Ironman stakes, orbital mechanics, hybrid crew visuals, controls, modding, and phased roadmap. |
| [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) | **Technical Architecture Blueprint** — C/Lua boundary, 64-bit cube-sphere & Keplerian orbital engine, nested interior reference frames, external VFS (desktop & Web), unified OpenGL/WebGL2/WebXR pipeline, input switching, and >= 95% test/coverage + automated game test design. |
| [`openspec/config.yaml`](openspec/config.yaml) | **OpenSpec Configuration** — spec-driven workflow configuration, confirmed design pillars, and verification rules. |

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
├── assets/                  # External CC BY 4.0 assets (models, textures, sprites, voxels, audio, planets) + MIT shaders
├── scripts/                 # External MIT Lua 5.4 gameplay scripts (factions, units, crew, AI, UI, rules, missions)
└── mods/                    # External user mod packages layered over assets/ and scripts/
```

---

## Project Status

**Current Stage:** Phase 0 Concept & Architecture Finalized — Ready for Phase 1 OpenSpec Change Proposal (`Citadel & Dropship` Vertical Slice).

---

## Licensing

- **Code, Lua Scripts, Shaders, Tests, and Documentation:** [MIT License](LICENSE-CODE)
- **Art and Audio Assets:** [Creative Commons Attribution 4.0 International (CC BY 4.0)](LICENSE-ART)

See [`LICENSE`](LICENSE) for exact scope boundaries.
