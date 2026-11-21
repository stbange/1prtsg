/* SPDX-License-Identifier: MIT */
#include "rts/crew.h"
#include <math.h>
#include <string.h>

void rts_crew_init(RtsCrewDeckSim *sim) {
    if (!sim) return;
    memset(sim, 0, sizeof(*sim));
}

const char *rts_crew_role_name(RtsCrewRoleId role) {
    switch (role) {
        case RTS_ROLE_OFFICER:       return "Command Officer";
        case RTS_ROLE_PILOT:         return "Helmsman / Pilot";
        case RTS_ROLE_ASTROGATOR:    return "Orbital Astrogator";
        case RTS_ROLE_GUNNER:        return "Weapons Gunner";
        case RTS_ROLE_SHIELD_TECH:   return "Shield Technician";
        case RTS_ROLE_COMMS_OFFICER: return "Comms & Sensor Officer";
        case RTS_ROLE_ENGINEER:      return "Chief Engineer";
        case RTS_ROLE_MEDIC:         return "Field Medic";
        case RTS_ROLE_MARINE:        return "Solar Marine";
        case RTS_ROLE_HANGAR_CREW:   return "Flight Deck Crew";
        case RTS_ROLE_MECHANIC:      return "Hull Mechanic";
        case RTS_ROLE_QUARTERMASTER: return "Logistics Officer";
        default:                     return "Crew Specialist";
    }
}

RtsVec4f rts_crew_role_tint(RtsCrewRoleId role, uint8_t faction_id) {
    if (faction_id == 2u) {
        return (RtsVec4f){1.0f, 0.42f, 0.50f, 1.0f}; /* Hostile Crimson Breacher */
    }
    switch (role) {
        case RTS_ROLE_OFFICER:       return (RtsVec4f){1.0f, 0.90f, 0.56f, 1.0f}; /* Command Champagne Gold */
        case RTS_ROLE_PILOT:
        case RTS_ROLE_ASTROGATOR:    return (RtsVec4f){0.62f, 0.96f, 1.0f, 1.0f}; /* Flight Electric Cyan */
        case RTS_ROLE_ENGINEER:
        case RTS_ROLE_MECHANIC:      return (RtsVec4f){1.0f, 0.78f, 0.42f, 1.0f}; /* Engineering Amber */
        case RTS_ROLE_SHIELD_TECH:
        case RTS_ROLE_COMMS_OFFICER: return (RtsVec4f){0.74f, 0.86f, 1.0f, 1.0f}; /* Tech Cerulean */
        case RTS_ROLE_MEDIC:         return (RtsVec4f){0.84f, 1.0f, 0.94f, 1.0f}; /* Medical White-Mint */
        case RTS_ROLE_MARINE:        return (RtsVec4f){0.54f, 0.98f, 0.76f, 1.0f}; /* Marine Emerald */
        case RTS_ROLE_GUNNER:        return (RtsVec4f){0.98f, 0.68f, 0.52f, 1.0f}; /* Ordnance Coral-Gold */
        default:                     return (RtsVec4f){0.90f, 0.95f, 1.0f, 1.0f}; /* Satin Titanium */
    }
}

static RtsCrewRoleId role_for_room_slot(RtsRoomKind kind, int slot) {
    switch (kind) {
        case RTS_ROOM_WAR_ROOM: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_OFFICER, RTS_ROLE_ASTROGATOR, RTS_ROLE_COMMS_OFFICER, RTS_ROLE_MARINE};
            return r[slot & 3];
        }
        case RTS_ROOM_BRIDGE_CIC: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_PILOT, RTS_ROLE_OFFICER, RTS_ROLE_ASTROGATOR, RTS_ROLE_COMMS_OFFICER};
            return r[slot & 3];
        }
        case RTS_ROOM_REACTOR:
        case RTS_ROOM_ENGINEERING: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_ENGINEER, RTS_ROLE_MECHANIC, RTS_ROLE_SHIELD_TECH, RTS_ROLE_ENGINEER};
            return r[slot & 3];
        }
        case RTS_ROOM_SHIELD_CORE: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_SHIELD_TECH, RTS_ROLE_ENGINEER, RTS_ROLE_MECHANIC, RTS_ROLE_SHIELD_TECH};
            return r[slot & 3];
        }
        case RTS_ROOM_HANGAR: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_HANGAR_CREW, RTS_ROLE_PILOT, RTS_ROLE_MECHANIC, RTS_ROLE_QUARTERMASTER};
            return r[slot & 3];
        }
        case RTS_ROOM_WEAPONS: {
            static const RtsCrewRoleId r[4] = {RTS_ROLE_GUNNER, RTS_ROLE_GUNNER, RTS_ROLE_OFFICER, RTS_ROLE_MECHANIC};
            return r[slot & 3];
        }
        case RTS_ROOM_MEDBAY:
            return (slot == 0) ? RTS_ROLE_MEDIC : RTS_ROLE_OFFICER;
        case RTS_ROOM_ARMORY:
        case RTS_ROOM_BARRACKS:
        case RTS_ROOM_AIRLOCK:
            return RTS_ROLE_MARINE;
        case RTS_ROOM_SENSOR_LAB:
            return (slot == 0) ? RTS_ROLE_COMMS_OFFICER : RTS_ROLE_ASTROGATOR;
        case RTS_ROOM_CARGO:
            return (slot == 0) ? RTS_ROLE_QUARTERMASTER : RTS_ROLE_HANGAR_CREW;
        case RTS_ROOM_CORRIDOR:
        case RTS_ROOM_TURBOLIFT:
        case RTS_ROOM_TRAM_STATION:
            return (slot == 0) ? RTS_ROLE_MARINE : RTS_ROLE_ENGINEER;
        default:
            return (RtsCrewRoleId)(slot % RTS_ROLE_COUNT);
    }
}

static bool add_agent(RtsCrewDeckSim *sim, RtsCrewRoleId role, uint8_t faction_id,
                      uint32_t home_room_id, RtsVec3d pos, double yaw, RtsCrewBehaviorState st) {
    if (!sim || sim->count >= RTS_MAX_ACTIVE_CREW) {
        return false;
    }
    RtsCrewAgent *a = &sim->agents[sim->count];
    memset(a, 0, sizeof(*a));
    a->id = (uint32_t)(sim->count + 1u);
    a->role = role;
    a->faction_id = faction_id;
    a->home_room_id = home_room_id;
    a->target_room_id = home_room_id;
    a->local_pos = pos;
    a->target_pos = pos;
    a->yaw_rad = yaw;
    a->walk_speed_mps = (role == RTS_ROLE_MARINE) ? 3.2 : 2.3;
    a->state = st;
    a->health = 100.0f;
    a->anim_phase = (float)sim->count * 0.7f;
    a->active = true;
    sim->count++;
    if (faction_id == 1u) {
        sim->distinct_role_mask |= (1u << (unsigned)role);
    }
    return true;
}

size_t rts_crew_populate_deck(RtsCrewDeckSim *sim, const RtsMegaInterior *mega, const RtsWorld *w) {
    if (!sim || !mega || !w) {
        return 0u;
    }
    rts_crew_init(sim);
    sim->entity_id = mega->entity_id;
    sim->active_deck = mega->active_deck;

    const RtsEntity *ent = rts_world_get_entity_const(w, mega->entity_id);
    if (!ent || ent->own_frame_id == RTS_FRAME_INVALID) {
        return 0u;
    }

    RtsFrameId fid = ent->own_frame_id;
    static const double offsets[4][2] = {
        {-2.6, -1.6}, { 2.6, -1.5}, {-2.2,  1.8}, { 2.4,  1.6}
    };
    static const double yaws[4] = {0.65, -0.65, 2.50, -2.45};

    for (size_t i = 0; i < w->interiors.room_count && sim->count < RTS_MAX_ACTIVE_CREW; ++i) {
        const RtsInteriorRoom *rm = &w->interiors.rooms[i];
        if (!rm->active || rm->frame_id != fid) continue;

        int per_room = 2;
        if (rm->kind == RTS_ROOM_WAR_ROOM || rm->kind == RTS_ROOM_BRIDGE_CIC ||
            rm->kind == RTS_ROOM_HANGAR || rm->kind == RTS_ROOM_REACTOR) {
            per_room = (mega->total_crew_capacity >= 20u) ? 4 : 2;
        } else if (mega->total_crew_capacity <= 3u) {
            per_room = 1;
        } else if (rm->kind == RTS_ROOM_CORRIDOR || rm->kind == RTS_ROOM_TURBOLIFT) {
            per_room = 2;
        }

        double cx = 0.5 * (rm->local_bounds.min.x + rm->local_bounds.max.x);
        double cz = 0.5 * (rm->local_bounds.min.z + rm->local_bounds.max.z);
        double half_x = 0.5 * (rm->local_bounds.max.x - rm->local_bounds.min.x) - 1.1;
        double half_z = 0.5 * (rm->local_bounds.max.z - rm->local_bounds.min.z) - 1.1;
        if (half_x < 0.8) half_x = 0.8;
        if (half_z < 0.8) half_z = 0.8;

        for (int s = 0; s < per_room && sim->count < RTS_MAX_ACTIVE_CREW; ++s) {
            RtsCrewRoleId role = role_for_room_slot(rm->kind, s);
            double ox = rts_clampd(offsets[s][0], -half_x, half_x);
            double oz = rts_clampd(offsets[s][1], -half_z, half_z);
            RtsCrewBehaviorState st = (role == RTS_ROLE_MARINE && (rm->kind == RTS_ROOM_CORRIDOR || rm->kind == RTS_ROOM_TURBOLIFT))
                                      ? RTS_CREW_PATROLLING : RTS_CREW_AT_STATION;
            add_agent(sim, role, 1u, rm->id, rts_v3d(cx + ox, 0.0, cz + oz), yaws[s], st);
        }
    }
    return sim->count;
}

void rts_crew_set_red_alert(RtsCrewDeckSim *sim, RtsMegaInterior *mega, RtsWorld *w, bool enable) {
    if (!sim || !mega || !w) return;
    sim->red_alert = enable;
    const RtsEntity *ent = rts_world_get_entity_const(w, mega->entity_id);
    if (!ent || ent->own_frame_id == RTS_FRAME_INVALID) return;
    RtsFrameId fid = ent->own_frame_id;

    if (enable) {
        /* Damage a key engineering/reactor room so Engineers physically sprint to repair it */
        uint32_t dmg_room_id = 0u;
        RtsVec3d airlock_pos = rts_v3d(0.0, 0.0, 0.0);
        uint32_t airlock_id = 0u;
        for (size_t i = 0; i < w->interiors.room_count; ++i) {
            RtsInteriorRoom *rm = &w->interiors.rooms[i];
            if (!rm->active || rm->frame_id != fid) continue;
            if (rm->kind == RTS_ROOM_REACTOR || rm->kind == RTS_ROOM_ENGINEERING || rm->kind == RTS_ROOM_SHIELD_CORE) {
                rm->damaged_pct = 58.0f;
                dmg_room_id = rm->id;
            }
            if (rm->kind == RTS_ROOM_AIRLOCK || rm->kind == RTS_ROOM_CORRIDOR) {
                airlock_id = rm->id;
                airlock_pos = rts_v3d(0.5 * (rm->local_bounds.min.x + rm->local_bounds.max.x),
                                      0.0,
                                      0.5 * (rm->local_bounds.min.z + rm->local_bounds.max.z));
            }
        }
        if (dmg_room_id == 0u && w->interiors.room_count > 0u) {
            for (size_t i = 0; i < w->interiors.room_count; ++i) {
                if (w->interiors.rooms[i].active && w->interiors.rooms[i].frame_id == fid) {
                    w->interiors.rooms[i].damaged_pct = 50.0f;
                    break;
                }
            }
        }
        /* Spawn a 3-breacher Crimson Syndicate boarding team if slots allow */
        if (airlock_id != 0u) {
            for (int b = 0; b < 3; ++b) {
                add_agent(sim, RTS_ROLE_MARINE, 2u, airlock_id,
                          rts_v3d(airlock_pos.x + (double)(b - 1) * 1.4, 0.0, airlock_pos.z),
                          0.0, RTS_CREW_COMBAT);
            }
        }
    } else {
        for (size_t i = 0; i < sim->count; ++i) {
            if (sim->agents[i].faction_id == 2u) {
                sim->agents[i].active = false;
            }
        }
    }
}

static const RtsInteriorRoom *pick_next_patrol_room(const RtsWorld *w, RtsFrameId fid, uint32_t seed) {
    const RtsInteriorRoom *cands[64];
    size_t n = 0;
    for (size_t i = 0; i < w->interiors.room_count && n < 64u; ++i) {
        const RtsInteriorRoom *rm = &w->interiors.rooms[i];
        if (rm->active && rm->frame_id == fid) {
            cands[n++] = rm;
        }
    }
    return n > 0u ? cands[seed % n] : NULL;
}

void rts_crew_step(RtsCrewDeckSim *sim, RtsMegaInterior *mega, RtsWorld *w, double dt_s) {
    if (!sim || !mega || !w || dt_s <= 0.0) return;
    const RtsEntity *ent = rts_world_get_entity_const(w, mega->entity_id);
    if (!ent || ent->own_frame_id == RTS_FRAME_INVALID) return;
    RtsFrameId fid = ent->own_frame_id;

    /* Locate first damaged room on the active deck for Engineer damage control */
    RtsInteriorRoom *damaged_room = NULL;
    for (size_t i = 0; i < w->interiors.room_count; ++i) {
        RtsInteriorRoom *rm = &w->interiors.rooms[i];
        if (rm->active && rm->frame_id == fid && rm->damaged_pct > 0.0f) {
            damaged_room = rm;
            break;
        }
    }

    /* Locate active hostile breacher for Marine counter-boarding */
    RtsCrewAgent *hostile = NULL;
    for (size_t i = 0; i < sim->count; ++i) {
        if (sim->agents[i].active && sim->agents[i].faction_id == 2u) {
            hostile = &sim->agents[i];
            break;
        }
    }

    for (size_t i = 0; i < sim->count; ++i) {
        RtsCrewAgent *a = &sim->agents[i];
        if (!a->active) continue;
        a->anim_phase += (float)dt_s * 3.5f;

        if (a->faction_id == 2u) {
            /* Breachers advance toward Bridge/War Room at (0, 0, -5) */
            a->target_pos = rts_v3d(0.0, 0.0, -5.0);
        } else if ((a->role == RTS_ROLE_ENGINEER || a->role == RTS_ROLE_MECHANIC) && damaged_room != NULL) {
            a->state = RTS_CREW_REPAIRING;
            a->target_room_id = damaged_room->id;
            a->target_pos = rts_v3d(0.5 * (damaged_room->local_bounds.min.x + damaged_room->local_bounds.max.x),
                                    0.0,
                                    0.5 * (damaged_room->local_bounds.min.z + damaged_room->local_bounds.max.z));
        } else if (a->role == RTS_ROLE_MARINE && hostile != NULL) {
            a->state = RTS_CREW_COMBAT;
            a->target_pos = hostile->local_pos;
        } else if (a->state == RTS_CREW_PATROLLING) {
            if (rts_v3d_dist(a->local_pos, a->target_pos) < 0.8) {
                const RtsInteriorRoom *nxt = pick_next_patrol_room(w, fid, (uint32_t)(a->id * 17u + (uint32_t)(a->anim_phase * 5.0f)));
                if (nxt) {
                    a->target_room_id = nxt->id;
                    a->target_pos = rts_v3d(0.5 * (nxt->local_bounds.min.x + nxt->local_bounds.max.x),
                                            0.0,
                                            0.5 * (nxt->local_bounds.min.z + nxt->local_bounds.max.z));
                }
            }
        } else if (a->state == RTS_CREW_REPAIRING && damaged_room == NULL) {
            a->state = RTS_CREW_AT_STATION;
        } else if (a->state == RTS_CREW_COMBAT && hostile == NULL) {
            a->state = RTS_CREW_PATROLLING;
        }

        if (a->state != RTS_CREW_AT_STATION) {
            RtsVec3d diff = rts_v3d_sub(a->target_pos, a->local_pos);
            diff.y = 0.0;
            double d = rts_v3d_len(diff);
            if (d > 0.35) {
                double step = a->walk_speed_mps * dt_s;
                if (step > d) step = d;
                a->local_pos = rts_v3d_add(a->local_pos, rts_v3d_scale(diff, step / d));
                a->yaw_rad = atan2(diff.x, diff.z);
            } else if (a->state == RTS_CREW_REPAIRING && damaged_room != NULL) {
                damaged_room->damaged_pct -= (float)(22.0 * dt_s);
                if (damaged_room->damaged_pct <= 0.0f) {
                    damaged_room->damaged_pct = 0.0f;
                    sim->repairs_completed++;
                    damaged_room = NULL;
                }
            }

            if (a->faction_id == 1u && a->state == RTS_CREW_COMBAT && hostile != NULL && d < 6.5) {
                hostile->health -= (float)(34.0 * dt_s);
                if (hostile->health <= 0.0f) {
                    hostile->active = false;
                    sim->breachers_neutralized++;
                    hostile = NULL;
                }
            }
        }
    }
}
