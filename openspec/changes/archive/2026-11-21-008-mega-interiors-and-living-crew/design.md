# Design: Mega-Interiors, Active-Deck Streaming & 12-Role Living Crew

## Memory & Stack Safety Architecture
- `RtsMegaInterior` (up to `RTS_MEGA_MAX_ROOMS = 512` rooms, `RTS_MEGA_MAX_DECKS = 24`) is heap-allocated (`calloc`) or owned inside `InteractiveApp` / `RtsWorld` companion state so `RtsWorld` stays compact and stack-safe on Windows MSVC and Emscripten WASM.
- Active-deck streaming hydrates only the Commander's active deck (`~1–24` rooms) into `world->interiors` on entry or deck change (`rts_mega_hydrate_deck`), keeping `RtsInteriorSystem` fast (`O(rooms_on_deck)`) and draw calls well within 60 FPS WebGL 2 budgets.

## Room Connectivity Invariant
- Each deck is generated on an orthogonal room grid (`cols x rows`, e.g., `1x1` for Starfighter up to `7x3 = 21` rooms per deck × `20` decks = `420` rooms for Orbital Shipyard).
- Every cell on a deck shares wall-to-wall aligned `RtsAabb` bounds (`room_w = 10m`, `room_d = 10m`, `deck_h = 4.2m`) and carries a `door_mask` (`N=1, E=2, S=4, W=8`) guaranteeing every room on the deck is reachable via open doorways from the deck's central `TURBOLIFT` hub (verified by BFS in unit tests across all 76 blueprints).
- In `rts_controller_step`, stepping across a shared boundary into an adjacent room succeeds seamlessly whenever `rts_interior_find_room_at(..., next_pos)` resolves to a valid room; perimeter walls clamp cleanly.

## 12-Role Crew Staffing
- `RtsCrewRoster` (`RTS_MAX_ACTIVE_CREW = 96` active agents on the hydrated deck) assigns role-accurate staff to every room on the active deck proportional to the construct's `crew_capacity`, with deterministic role uniforms, console posts, multi-room corridor patrol waypoints, and Red Alert / repair AI.
