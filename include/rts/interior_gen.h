/* SPDX-License-Identifier: MIT */
#ifndef RTS_INTERIOR_GEN_H
#define RTS_INTERIOR_GEN_H

#include "rts/interior.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MEGA_MAX_ROOMS 512
#define RTS_MEGA_MAX_DECKS 24

typedef struct RtsMegaRoomSpec {
    uint16_t room_index;
    uint16_t deck_index;
    int16_t grid_x;
    int16_t grid_z;
    RtsRoomKind kind;
    uint8_t door_mask;
    char name[64];
    RtsAabb local_bounds;
    float damaged_pct;
    bool breached;
} RtsMegaRoomSpec;

typedef struct RtsMegaInterior {
    char blueprint_id[64];
    uint32_t entity_id;
    uint16_t deck_count;
    uint16_t cols;
    uint16_t rows;
    uint16_t active_deck;
    uint32_t total_rooms;
    uint32_t total_crew_capacity;
    float room_w_m;
    float room_d_m;
    float deck_h_m;
    bool has_holo_globe;
    bool has_escape_pod;
    bool can_orbit;
    RtsConstructClass construct_class;
    RtsMegaRoomSpec rooms[RTS_MEGA_MAX_ROOMS];
} RtsMegaInterior;

/* Deterministically generates a 1..420 room multi-deck floorplan for any of the 76 blueprints */
bool rts_mega_generate_for_blueprint(RtsMegaInterior *out_mega, const RtsConstructBlueprint *bp, uint32_t entity_id);

/* Verifies via BFS that 100% of rooms on every deck are reachable via open bulkhead doorways */
bool rts_mega_verify_connectivity(const RtsMegaInterior *mega, uint32_t *out_reachable_total);

/* Streams (hydrates) only deck_index into world->interiors for entity_id's own_frame_id */
bool rts_mega_hydrate_deck(RtsMegaInterior *mega, RtsWorld *w, uint32_t entity_id, uint16_t deck_index, RtsVec3d *out_spawn_pos);

/* Switches Commander vertically to target_deck via Turbolift and hydrates destination deck */
bool rts_mega_switch_deck(RtsMegaInterior *mega, RtsWorld *w, uint16_t target_deck);

/* Returns human-readable name for a room kind */
const char *rts_room_kind_name(RtsRoomKind kind);

/* Finds the deck index containing the first room matching kind (or 0 if none) */
uint16_t rts_mega_find_deck_with_kind(const RtsMegaInterior *mega, RtsRoomKind kind);

#endif /* RTS_INTERIOR_GEN_H */
