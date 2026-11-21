/* SPDX-License-Identifier: MIT */
#include "rts/economy.h"
#include <math.h>
#include <string.h>

void rts_economy_init(RtsEconomySystem *eco) {
    memset(eco, 0, sizeof(*eco));
    eco->next_job_id = 1u;
    for (int f = 0; f < RTS_MAX_FACTIONS; ++f) {
        eco->ledgers[f].ore = 4000.0;
        eco->ledgers[f].fuel = 2000.0;
        eco->ledgers[f].power_produced = 500.0;
        eco->ledgers[f].power_consumed = 100.0;
        eco->ledgers[f].manpower_cap = 400u;
        eco->ledgers[f].manpower_used = 48u;
    }
}

void rts_economy_add_resources(RtsEconomySystem *eco, uint8_t faction_id, double ore, double fuel, double power, uint32_t manpower) {
    if (!eco || faction_id >= RTS_MAX_FACTIONS) return;
    eco->ledgers[faction_id].ore += ore;
    eco->ledgers[faction_id].fuel += fuel;
    eco->ledgers[faction_id].power_produced += power;
    eco->ledgers[faction_id].manpower_cap += manpower;
}

void rts_economy_blueprint_cost(const RtsConstructBlueprint *bp, double *out_ore, double *out_fuel, double *out_build_s) {
    double ore = 100.0, fuel = 40.0, time_s = 5.0;
    if (bp) {
        ore = (double)bp->max_hull * 0.35 + (double)bp->max_shield * 0.20;
        fuel = bp->can_orbit ? ore * 0.35 : ore * 0.15;
        time_s = rts_clampd(3.0 + ore * 0.008, 3.0, 90.0);
    }
    if (out_ore) *out_ore = ore;
    if (out_fuel) *out_fuel = fuel;
    if (out_build_s) *out_build_s = time_s;
}

uint32_t rts_economy_queue_build(RtsEconomySystem *eco, const RtsWorld *w, uint32_t producer_entity_id, const char *blueprint_id) {
    if (!eco || !w || !blueprint_id || eco->job_count >= RTS_MAX_PROD_JOBS) {
        return 0u;
    }
    const RtsEntity *prod = rts_world_get_entity_const(w, producer_entity_id);
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, blueprint_id);
    if (!prod || !bp || prod->faction_id >= RTS_MAX_FACTIONS) {
        return 0u;
    }
    double cost_ore = 0.0, cost_fuel = 0.0, build_s = 0.0;
    rts_economy_blueprint_cost(bp, &cost_ore, &cost_fuel, &build_s);
    RtsFactionLedger *led = &eco->ledgers[prod->faction_id];
    if (led->ore < cost_ore || led->fuel < cost_fuel || (led->manpower_used + bp->crew_capacity) > led->manpower_cap) {
        return 0u;
    }
    led->ore -= cost_ore;
    led->fuel -= cost_fuel;
    led->manpower_used += bp->crew_capacity;

    RtsProductionJob *j = &eco->jobs[eco->job_count++];
    memset(j, 0, sizeof(*j));
    j->id = eco->next_job_id++;
    j->producer_entity_id = producer_entity_id;
    j->faction_id = prod->faction_id;
    strncpy(j->blueprint_id, bp->id, sizeof(j->blueprint_id) - 1);
    j->total_time_s = build_s;
    j->remaining_s = build_s;
    j->active = true;
    return j->id;
}

size_t rts_economy_step(RtsEconomySystem *eco, RtsWorld *w, double dt_s) {
    if (!eco || !w || dt_s <= 0.0) return 0u;
    /* Passive extraction & refining income from active mines/refineries/plants */
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active || e->faction_id >= RTS_MAX_FACTIONS) continue;
        if (strcmp(e->blueprint_id, "bldg_ore_mine") == 0 || strcmp(e->blueprint_id, "bldg_deep_drill") == 0) {
            eco->ledgers[e->faction_id].ore += 35.0 * dt_s;
        } else if (strcmp(e->blueprint_id, "bldg_fuel_plant") == 0 || strcmp(e->blueprint_id, "stn_fuel_ore_depot") == 0) {
            eco->ledgers[e->faction_id].fuel += 25.0 * dt_s;
        }
    }

    size_t completed_now = 0u;
    for (size_t i = 0; i < eco->job_count; ++i) {
        RtsProductionJob *j = &eco->jobs[i];
        if (!j->active || j->completed) continue;
        const RtsEntity *prod = rts_world_get_entity_const(w, j->producer_entity_id);
        if (!prod) {
            j->active = false;
            continue;
        }
        RtsFactionLedger *led = &eco->ledgers[j->faction_id];
        double power_mult = (led->power_produced >= led->power_consumed) ? 1.0 : 0.4;
        j->remaining_s -= dt_s * power_mult;
        if (j->remaining_s <= 0.0) {
            j->remaining_s = 0.0;
            j->completed = true;
            RtsVec3d spawn_pos = rts_v3d_add(prod->pos, rts_v3d(35.0, 0.0, 15.0));
            j->spawned_entity_id = rts_world_spawn_from_blueprint(w, j->blueprint_id, NULL, j->faction_id, prod->host_frame_id, spawn_pos, true);
            if (prod->orbital_mode && j->spawned_entity_id != 0u) {
                RtsEntity *sp = rts_world_get_entity(w, j->spawned_entity_id);
                if (sp) {
                    sp->vel = prod->vel;
                    sp->orbital_mode = true;
                }
            }
            completed_now++;
        }
    }
    return completed_now;
}

const RtsProductionJob *rts_economy_get_job(const RtsEconomySystem *eco, uint32_t job_id) {
    if (!eco || job_id == 0u) return NULL;
    for (size_t i = 0; i < eco->job_count; ++i) {
        if (eco->jobs[i].id == job_id) {
            return &eco->jobs[i];
        }
    }
    return NULL;
}

bool rts_planet_has_los(const RtsPlanetConfig *planet, RtsVec3d a, RtsVec3d b) {
    if (!planet) return true;
    /* Closest point on segment [a, b] to planet center (0,0,0) must stay above surface radius (with 50m tolerance) */
    RtsVec3d ab = rts_v3d_sub(b, a);
    double len2 = rts_v3d_len_sq(ab);
    double t = 0.0;
    if (len2 > 1e-9) {
        t = rts_clampd(-rts_v3d_dot(a, ab) / len2, 0.0, 1.0);
    }
    RtsVec3d closest = rts_v3d_add(a, rts_v3d_scale(ab, t));
    double min_r = planet->radius_m - 50.0;
    return rts_v3d_len(closest) >= min_r;
}

double rts_entity_sensor_range_m(const RtsEntity *e) {
    if (!e || !e->active || e->subsystem_hp[RTS_SUBSYS_SENSORS] < 15.0f) return 0.0;
    if (strcmp(e->blueprint_id, "stn_recon_sat") == 0) return 3500000.0;
    if (strcmp(e->blueprint_id, "bldg_radar_array") == 0) return 650000.0;
    if (strcmp(e->blueprint_id, "air_awacs_cruiser") == 0) return 400000.0;
    if (e->construct_class == RTS_CLASS_SPACE_STATION || e->construct_class == RTS_CLASS_SPACECRAFT) return 900000.0;
    if (e->construct_class == RTS_CLASS_NAVAL_SHIP || e->construct_class == RTS_CLASS_BUILDING) return 80000.0;
    return 25000.0;
}

double rts_entity_comms_range_m(const RtsEntity *e) {
    if (!e || !e->active || e->subsystem_hp[RTS_SUBSYS_SENSORS] < 15.0f) return 0.0;
    if (strcmp(e->blueprint_id, "stn_comms_relay") == 0) return 45000000.0;
    if (strcmp(e->blueprint_id, "bldg_comms_tower") == 0) return 15000000.0;
    if (strcmp(e->blueprint_id, "ship_relay_tender") == 0) return 6000000.0;
    if (strcmp(e->blueprint_id, "bldg_command_citadel") == 0 || strcmp(e->blueprint_id, "stn_deep_space_citadel") == 0) return 10000000.0;
    return 60000.0;
}

bool rts_sensors_is_target_detected(const RtsWorld *w, uint8_t observer_faction, uint32_t target_entity_id) {
    const RtsEntity *tgt = rts_world_get_entity_const(w, target_entity_id);
    if (!tgt) return false;
    if (tgt->faction_id == observer_faction) return true;
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *obs = &w->entities[i];
        if (!obs->active || obs->faction_id != observer_faction) continue;
        double range = rts_entity_sensor_range_m(obs);
        if (range <= 0.0) continue;
        if (rts_v3d_dist(obs->pos, tgt->pos) <= range && rts_planet_has_los(&w->planet, obs->pos, tgt->pos)) {
            return true;
        }
    }
    return false;
}

bool rts_comms_is_entity_linked(const RtsWorld *w, uint32_t hq_entity_id, uint32_t unit_entity_id) {
    const RtsEntity *hq = rts_world_get_entity_const(w, hq_entity_id);
    const RtsEntity *unit = rts_world_get_entity_const(w, unit_entity_id);
    if (!hq || !unit || hq->faction_id != unit->faction_id) return false;
    if (hq_entity_id == unit_entity_id) return true;

    bool visited[RTS_MAX_ENTITIES];
    uint32_t queue[RTS_MAX_ENTITIES];
    memset(visited, 0, sizeof(visited));
    size_t qh = 0, qt = 0;

    for (size_t i = 0; i < w->entity_count; ++i) {
        if (w->entities[i].active && w->entities[i].id == hq_entity_id) {
            visited[i] = true;
            queue[qt++] = (uint32_t)i;
            break;
        }
    }

    while (qh < qt) {
        size_t cur_idx = queue[qh++];
        const RtsEntity *cur = &w->entities[cur_idx];
        double cur_range = rts_entity_comms_range_m(cur);
        for (size_t j = 0; j < w->entity_count; ++j) {
            if (visited[j]) continue;
            const RtsEntity *nxt = &w->entities[j];
            if (!nxt->active || nxt->faction_id != hq->faction_id) continue;
            double max_r = fmax(cur_range, rts_entity_comms_range_m(nxt));
            if (rts_v3d_dist(cur->pos, nxt->pos) <= max_r && rts_planet_has_los(&w->planet, cur->pos, nxt->pos)) {
                if (nxt->id == unit_entity_id) {
                    return true;
                }
                visited[j] = true;
                queue[qt++] = (uint32_t)j;
            }
        }
    }
    return false;
}
