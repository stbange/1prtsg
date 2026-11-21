/* SPDX-License-Identifier: MIT */
#ifndef RTS_ECONOMY_H
#define RTS_ECONOMY_H

#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MAX_FACTIONS 4
#define RTS_MAX_PROD_JOBS 64

typedef struct RtsFactionLedger {
    double ore;
    double fuel;
    double power_produced;
    double power_consumed;
    uint32_t manpower_cap;
    uint32_t manpower_used;
} RtsFactionLedger;

typedef struct RtsProductionJob {
    uint32_t id;
    uint32_t producer_entity_id;
    uint8_t faction_id;
    char blueprint_id[64];
    double total_time_s;
    double remaining_s;
    uint32_t spawned_entity_id;
    bool completed;
    bool active;
} RtsProductionJob;

typedef struct RtsEconomySystem {
    RtsFactionLedger ledgers[RTS_MAX_FACTIONS];
    RtsProductionJob jobs[RTS_MAX_PROD_JOBS];
    size_t job_count;
    uint32_t next_job_id;
} RtsEconomySystem;

void rts_economy_init(RtsEconomySystem *eco);
void rts_economy_add_resources(RtsEconomySystem *eco, uint8_t faction_id, double ore, double fuel, double power, uint32_t manpower);
void rts_economy_blueprint_cost(const RtsConstructBlueprint *bp, double *out_ore, double *out_fuel, double *out_build_s);
uint32_t rts_economy_queue_build(RtsEconomySystem *eco, const RtsWorld *w, uint32_t producer_entity_id, const char *blueprint_id);
size_t rts_economy_step(RtsEconomySystem *eco, RtsWorld *w, double dt_s);
const RtsProductionJob *rts_economy_get_job(const RtsEconomySystem *eco, uint32_t job_id);

/* Spherical Planetary Horizon Line-of-Sight, Fog-of-War & Comms Relay Network */
bool rts_planet_has_los(const RtsPlanetConfig *planet, RtsVec3d a, RtsVec3d b);
double rts_entity_sensor_range_m(const RtsEntity *e);
double rts_entity_comms_range_m(const RtsEntity *e);
bool rts_sensors_is_target_detected(const RtsWorld *w, uint8_t observer_faction, uint32_t target_entity_id);
bool rts_comms_is_entity_linked(const RtsWorld *w, uint32_t hq_entity_id, uint32_t unit_entity_id);

#endif /* RTS_ECONOMY_H */
