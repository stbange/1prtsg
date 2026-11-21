/* SPDX-License-Identifier: MIT */
#ifndef RTS_CONTROLLER_H
#define RTS_CONTROLLER_H

#include "rts/input.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum RtsCommanderMode {
    RTS_CMD_ON_FOOT = 0,
    RTS_CMD_AT_HOLO_GLOBE = 1,
    RTS_CMD_AT_FACTORY = 2,
    RTS_CMD_PILOTING = 3
} RtsCommanderMode;

typedef enum RtsAutopilotPhase {
    RTS_AUTO_IDLE = 0,
    RTS_AUTO_ASCENT = 1,
    RTS_AUTO_CIRCULARIZED = 2,
    RTS_AUTO_RENDEZVOUS = 3,
    RTS_AUTO_REENTRY = 4,
    RTS_AUTO_LANDED = 5
} RtsAutopilotPhase;

typedef struct RtsAstrogatorPlan {
    uint32_t craft_entity_id;
    uint32_t target_station_id;
    double target_altitude_m;
    RtsAutopilotPhase phase;
    double peak_reentry_heat;
    bool docked;
} RtsAstrogatorPlan;

typedef struct RtsCommanderController {
    RtsCommanderMode mode;
    uint32_t active_console_id;
    uint32_t piloted_entity_id;
    double walk_speed_mps;
    double pilot_throttle;
    double interact_buffer_s;
    RtsAstrogatorPlan astrogator;
} RtsCommanderController;

void rts_controller_init(RtsCommanderController *ctrl);
void rts_controller_step(RtsCommanderController *ctrl, RtsWorld *w, const RtsInputIntent *intent, double dt_s);
bool rts_controller_engage_piloting(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id);
void rts_controller_release_piloting(RtsCommanderController *ctrl);

/* Automated Astrogator Orbital Flight */
bool rts_astrogator_start_ascent(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id, double target_alt_m);
bool rts_astrogator_start_rendezvous(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id, uint32_t station_id);
bool rts_astrogator_start_reentry(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id);
RtsAutopilotPhase rts_astrogator_step(RtsCommanderController *ctrl, RtsWorld *w, double dt_s);

#endif /* RTS_CONTROLLER_H */
