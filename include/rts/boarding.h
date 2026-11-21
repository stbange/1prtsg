/* SPDX-License-Identifier: MIT */
#ifndef RTS_BOARDING_H
#define RTS_BOARDING_H

#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MAX_BOARDING_OPS 32
#define RTS_MAX_MELTDOWNS    32

typedef enum RtsBoardingDeck {
    RTS_DECK_HANGAR_AIRLOCK = 0,
    RTS_DECK_ENGINEERING    = 1,
    RTS_DECK_BRIDGE_CIC     = 2
} RtsBoardingDeck;

typedef enum RtsBoardingStatus {
    RTS_BOARD_ACTIVE = 0,
    RTS_BOARD_CAPTURED = 1,
    RTS_BOARD_REPELLED = 2,
    RTS_BOARD_ABORTED_DESTROYED = 3
} RtsBoardingStatus;

typedef struct RtsBoardingOp {
    uint32_t id;
    uint32_t target_entity_id;
    uint8_t attacker_faction;
    RtsBoardingDeck current_deck;
    RtsBoardingStatus status;
    uint32_t attackers;
    uint32_t defenders;
    bool decompressed;
    bool attackers_mag_boots;
    double deck_progress; /* 0.0 -> 1.0 per deck */
    bool active;
} RtsBoardingOp;

typedef struct RtsMeltdownEvent {
    uint32_t entity_id;
    double countdown_s;
    double initial_s;
    bool active;
    bool detonated;
} RtsMeltdownEvent;

typedef struct RtsBoardingSystem {
    RtsBoardingOp ops[RTS_MAX_BOARDING_OPS];
    size_t op_count;
    uint32_t next_op_id;
    RtsMeltdownEvent meltdowns[RTS_MAX_MELTDOWNS];
    size_t meltdown_count;
} RtsBoardingSystem;

void rts_boarding_init(RtsBoardingSystem *bs);

/* Launch a marine boarding assault onto an enemy construct */
uint32_t rts_boarding_start(RtsBoardingSystem *bs, const RtsWorld *w, uint32_t target_entity_id, uint8_t attacker_faction, uint32_t marine_count, bool mag_boots);

/* Breach bulkhead & decompress target deck (vacuum hazard penalizes non-mag-boot personnel) */
bool rts_boarding_set_decompression(RtsBoardingSystem *bs, uint32_t op_id, bool decompressed);

const RtsBoardingOp *rts_boarding_get_op(const RtsBoardingSystem *bs, uint32_t op_id);

/* Initiate a Critical Reactor Meltdown countdown on an entity (scuttling or critical core hit) */
bool rts_boarding_trigger_meltdown(RtsBoardingSystem *bs, RtsWorld *w, uint32_t entity_id, double countdown_s);
const RtsMeltdownEvent *rts_boarding_get_meltdown(const RtsBoardingSystem *bs, uint32_t entity_id);

/* Advance all active boarding assaults and reactor meltdown timers by dt_s */
size_t rts_boarding_step(RtsBoardingSystem *bs, RtsWorld *w, double dt_s);

#endif
