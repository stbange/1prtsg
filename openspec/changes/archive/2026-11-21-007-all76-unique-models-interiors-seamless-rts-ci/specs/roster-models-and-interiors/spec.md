# Spec Delta

## ADDED Requirements

### Requirement: Dedicated 76-Construct Sculpted Mesh Catalog and Tailored Multi-Deck Interiors
Every one of the 76 constructs registered in external Lua blueprints SHALL reference its own unique sculpted external `.obj` model (`assets/models/<construct_id>.obj`) loaded dynamically via VFS mesh caching in `RtsRetroRenderer`, and SHALL provide a tailored multi-deck enterable interior layout (rooms, portals, and interactive consoles) in `scripts/interiors/catalog.lua`.

#### Scenario: Rendering all 76 unique constructs and hydrating tailored interiors
- **GIVEN** all 76 construct blueprints loaded from `scripts/units/*.lua` and their corresponding `assets/models/<id>.obj` files
- **WHEN** `RtsRetroRenderer` renders the 6-class gallery and live seamless planetary-to-orbital world and Lua hydrates construct interiors
- **THEN** all 76 unique `.obj` meshes load with zero missing files and zero OpenGL errors, and all 76 constructs hydrate valid multi-room enterable interiors

### Requirement: Seamless Live Planetary-to-Orbital Sandbox Mode and 5-Platform Clean Builds
The interactive SDL3/OpenGL application SHALL support seamless live RTS mode (`M`) combining interior walking, physical Holo-Globe order dispatch, Dropship piloting & Astrogator 400 km orbital ascent, boarding assault, and `F5`/`F9` deterministic `RTS_SAVE_V1` save/load, and SHALL compile cleanly across Linux GCC, Linux ASan/UBSan, macOS Clang, Windows MSVC, and Emscripten Web/WebXR.

#### Scenario: Live sandbox mode and cross-platform CI verification
- **GIVEN** the CMake build configuration and live SDL3/WebXR platform layer
- **WHEN** built and tested across Linux, macOS, Windows MSVC, and Emscripten WASM
- **THEN** all 5 CI pipelines succeed with zero baked assets and >= 95% C11 & Lua test coverage
