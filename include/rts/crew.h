/* SPDX-License-Identifier: MIT */
#ifndef RTS_CREW_H
#define RTS_CREW_H

#include "rts/interior_gen.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MAX_ACTIVE_CREW 96

typedef enum RtsCrewRoleId {
    RTS_ROLE_OFFICER = 0,
    RTS_ROLE_PILOT = 1,
    RTS_ROLE_ASTROGATOR = 2,
    RTS_ROLE_GUNNER = 3,
    RTS_ROLE_SHIELD_TECH = 4,
    RTS_ROLE_COMMS_OFFICER = 5,
    RTS_ROLE_ENGINEER = 6,
    RTS_ROLE_MEDIC = 7,
    RTS_ROLE_MARINE = 8,
    RTS_ROLE_HANGAR_CREW = 9,
    RTS_ROLE_MECHANIC = 10,
    RTS_ROLE_QUARTERMASTER = 11,
    RTS_ROLE_COUNT = 12
} RtsCrewRoleId;

typedef enum RtsCrewBehaviorState {
    RTS_CREW_AT_STATION = 0,
    RTS_CREW_PATROLLING = 1,
    RTS_CREW_REPAIRING = 2,
    RTS_CREW_COMBAT = 3
} RtsCrewBehaviorState;

typedef struct RtsCrewAgent {
    uint32_t id;
    RtsCrewRoleId role;
    uint8_t faction_id; /* 1 = Friendly Crew, 2 = Hostile Boarder */
    uint32_t home_room_id;
    uint32_t target_room_id;
    RtsVec3d local_pos;
    RtsVec3d target_pos;
    double yaw_rad;
    double walk_speed_mps;
    RtsCrewBehaviorState state;
    float health;
    float anim_phase;
    bool active;
} RtsCrewAgent;

typedef struct RtsCrewDeckSim {
    RtsCrewAgent agents[RTS_MAX_ACTIVE_CREW];
    size_t count;
    uint32_t entity_id;
    uint16_t active_deck;
    uint32_t distinct_role_mask;
    bool red_alert;
    uint32_t repairs_completed;
    uint32_t breachers_neutralized;
} RtsCrewDeckSim;

void rts_crew_init(RtsCrewDeckSim *sim);
const char *rts_crew_role_name(RtsCrewRoleId role);
RtsVec4f rts_crew_role_tint(RtsCrewRoleId role, uint8_t faction_id);

/* Populates every hydrated room on the active deck with role-accurate staffed crew */
size_t rts_crew_populate_deck(RtsCrewDeckSim *sim, const RtsMegaInterior *mega, const RtsWorld *w);

/* Triggers Red Alert: damages Reactor/Engineering for Engineer damage control + spawns Crimson Breachers */
void rts_crew_set_red_alert(RtsCrewDeckSim *sim, RtsMegaInterior *mega, RtsWorld *w, bool enable);

/* Advances crew patrols, station work, engineer damage repairs, and room-by-room marine defense */
void rts_crew_step(RtsCrewDeckSim *sim, RtsMegaInterior *mega, RtsWorld *w, double dt_s);

#endif /* RTS_CREW_H */
