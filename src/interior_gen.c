/* SPDX-License-Identifier: MIT */
#include "rts/interior_gen.h"
#include <stdio.h>
#include <string.h>

const char *rts_room_kind_name(RtsRoomKind kind) {
    switch (kind) {
        case RTS_ROOM_BRIDGE_CIC:  return "Bridge & CIC";
        case RTS_ROOM_WAR_ROOM:    return "War Room Holo-Deck";
        case RTS_ROOM_CORRIDOR:    return "Main Spine Corridor";
        case RTS_ROOM_TURBOLIFT:   return "Turbolift Hub";
        case RTS_ROOM_HANGAR:      return "Starfighter Hangar Bay";
        case RTS_ROOM_REACTOR:     return "Fusion Reactor Core";
        case RTS_ROOM_SHIELD_CORE: return "Shield Capacitor Core";
        case RTS_ROOM_WEAPONS:     return "Plasma Battery Deck";
        case RTS_ROOM_QUARTERS:    return "Crew Quarters";
        case RTS_ROOM_MESS_HALL:   return "Crew Mess & Lounge";
        case RTS_ROOM_MEDBAY:      return "Auto-Doc Medbay";
        case RTS_ROOM_ARMORY:      return "Tactical Armory";
        case RTS_ROOM_BARRACKS:    return "Marine Garrison";
        case RTS_ROOM_SENSOR_LAB:  return "Sensor & Comms Lab";
        case RTS_ROOM_CARGO:       return "Logistics Cargo Hold";
        case RTS_ROOM_ENGINEERING: return "Engineering Gallery";
        case RTS_ROOM_AIRLOCK:     return "Boarding Airlock";
        case RTS_ROOM_ESCAPE_POD:  return "Escape Pod Bay";
        case RTS_ROOM_TRAM_STATION:return "Internal Mag-Tram";
        default:                   return "Operations Deck";
    }
}

static uint32_t hash_u32(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return x;
}

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261u;
    while (s && *s) {
        h ^= (uint8_t)(*s++);
        h *= 16777619u;
    }
    return h;
}

static RtsRoomKind pick_room_kind(const RtsConstructBlueprint *bp, uint16_t deck, uint16_t decks,
                                  int gx, int gz, int cols, int rows, uint32_t seed) {
    int cx = cols / 2;
    int cz = rows / 2;

    if (cols == 1 && rows == 1) {
        return bp->has_holo_globe ? RTS_ROOM_WAR_ROOM : RTS_ROOM_BRIDGE_CIC;
    }
    if (decks > 1 && gx == cx && gz == cz) {
        return RTS_ROOM_TURBOLIFT;
    }

    /* Deck 0: Command, Bridge, War Room, Airlock & Escape Pods */
    if (deck == 0) {
        if (gx == cx && gz == 0) {
            return bp->has_holo_globe ? RTS_ROOM_WAR_ROOM : RTS_ROOM_BRIDGE_CIC;
        }
        if (gx == cx && gz == rows - 1) {
            return RTS_ROOM_AIRLOCK;
        }
        if (bp->has_escape_pod && gx == cols - 1 && gz == cz) {
            return RTS_ROOM_ESCAPE_POD;
        }
        if (gx == 0 && gz == 0) {
            return RTS_ROOM_SENSOR_LAB;
        }
        if (gx == cols - 1 && gz == 0) {
            return RTS_ROOM_BRIDGE_CIC;
        }
        if (gz == cz) {
            return RTS_ROOM_CORRIDOR;
        }
        static const RtsRoomKind cmd_wings[4] = {
            RTS_ROOM_ARMORY, RTS_ROOM_BARRACKS, RTS_ROOM_SENSOR_LAB, RTS_ROOM_MEDBAY
        };
        return cmd_wings[(hash_u32(seed ^ (uint32_t)(gx * 31 + gz * 17))) & 3u];
    }

    /* Hangar & Flight Deck tier (Deck 1 on multi-deck capital ships/stations/carriers) */
    if (decks >= 4 && deck == 1) {
        if (gx == 0 || gx == cols - 1) return RTS_ROOM_HANGAR;
        if (gz == cz) return (cols >= 5 && (gx == 1 || gx == cols - 2)) ? RTS_ROOM_TRAM_STATION : RTS_ROOM_CORRIDOR;
        return (gz == 0) ? RTS_ROOM_HANGAR : RTS_ROOM_CARGO;
    }

    /* Lower Engineering, Reactor Core & Shield Capacitor Decks */
    if (deck >= (uint16_t)((decks * 2) / 3)) {
        if (gx == cx && gz == 0) return RTS_ROOM_REACTOR;
        if (gx == cx && gz == rows - 1) return RTS_ROOM_SHIELD_CORE;
        if (bp->has_escape_pod && gx == cols - 1 && gz == rows - 1) return RTS_ROOM_ESCAPE_POD;
        if (gz == cz) return RTS_ROOM_CORRIDOR;
        static const RtsRoomKind eng_rooms[4] = {
            RTS_ROOM_ENGINEERING, RTS_ROOM_REACTOR, RTS_ROOM_SHIELD_CORE, RTS_ROOM_CARGO
        };
        return eng_rooms[(hash_u32(seed ^ (uint32_t)(deck * 97 + gx * 13 + gz * 7))) & 3u];
    }

    /* Mid Operations, Weapons, Marine Garrison, Quarters, Mess & Medbay Decks */
    if (gz == cz) {
        if (cols >= 5 && (gx == 0 || gx == cols - 1)) return RTS_ROOM_TRAM_STATION;
        return RTS_ROOM_CORRIDOR;
    }
    if (bp->has_escape_pod && gx == cols - 1 && gz == rows - 1 && (deck % 3 == 0)) {
        return RTS_ROOM_ESCAPE_POD;
    }
    static const RtsRoomKind mid_rooms[8] = {
        RTS_ROOM_WEAPONS, RTS_ROOM_QUARTERS, RTS_ROOM_MESS_HALL, RTS_ROOM_MEDBAY,
        RTS_ROOM_ARMORY, RTS_ROOM_BARRACKS, RTS_ROOM_SENSOR_LAB, RTS_ROOM_CARGO
    };
    return mid_rooms[(hash_u32(seed ^ (uint32_t)(deck * 53 + gx * 19 + gz * 29))) & 7u];
}

static RtsVec3f light_for_room_kind(RtsRoomKind kind) {
    switch (kind) {
        case RTS_ROOM_WAR_ROOM:
        case RTS_ROOM_BRIDGE_CIC:  return (RtsVec3f){0.62f, 0.90f, 1.0f};
        case RTS_ROOM_REACTOR:     return (RtsVec3f){0.40f, 0.96f, 1.0f};
        case RTS_ROOM_SHIELD_CORE: return (RtsVec3f){0.45f, 0.85f, 1.0f};
        case RTS_ROOM_MEDBAY:      return (RtsVec3f){0.72f, 1.0f, 0.92f};
        case RTS_ROOM_TURBOLIFT:   return (RtsVec3f){1.0f, 0.86f, 0.45f};
        case RTS_ROOM_ESCAPE_POD:  return (RtsVec3f){0.46f, 1.0f, 0.72f};
        default:                   return (RtsVec3f){0.76f, 0.86f, 0.98f};
    }
}

bool rts_mega_generate_for_blueprint(RtsMegaInterior *out_mega, const RtsConstructBlueprint *bp, uint32_t entity_id) {
    if (!out_mega || !bp || bp->id[0] == '\0') {
        return false;
    }
    memset(out_mega, 0, sizeof(*out_mega));
    snprintf(out_mega->blueprint_id, sizeof(out_mega->blueprint_id), "%s", bp->id);
    out_mega->entity_id = entity_id;
    out_mega->total_crew_capacity = bp->crew_capacity > 0u ? bp->crew_capacity : 2u;
    out_mega->has_holo_globe = bp->has_holo_globe;
    out_mega->has_escape_pod = bp->has_escape_pod;
    out_mega->can_orbit = bp->can_orbit;
    out_mega->construct_class = bp->construct_class;

    uint16_t decks = 1, cols = 1, rows = 1;
    float rw = 6.0f, rd = 6.0f, dh = 4.2f;

    if (bp->room_count <= 1u) {
        decks = 1; cols = 1; rows = 1; rw = 5.0f; rd = 5.5f;
    } else if (bp->room_count <= 3u) {
        decks = 1; cols = 1; rows = (uint16_t)bp->room_count; rw = 6.5f; rd = 7.0f;
    } else if (bp->room_count <= 6u) {
        decks = 2; cols = 3; rows = 2; rw = 8.5f; rd = 8.5f;
    } else if (bp->room_count <= 9u) {
        decks = 4; cols = 3; rows = 3; rw = 9.5f; rd = 9.5f;
    } else if (bp->room_count <= 12u) {
        decks = 6; cols = 4; rows = 3; rw = 10.0f; rd = 10.0f;
    } else if (bp->room_count <= 16u) {
        decks = 10; cols = 5; rows = 3; rw = 10.0f; rd = 10.0f;
    } else if (bp->room_count <= 20u) {
        decks = 14; cols = 5; rows = 4; rw = 10.0f; rd = 10.0f;
    } else {
        decks = 20; cols = 7; rows = 3; rw = 10.0f; rd = 10.0f;
    }

    if ((uint32_t)decks * (uint32_t)cols * (uint32_t)rows > RTS_MEGA_MAX_ROOMS) {
        decks = (uint16_t)(RTS_MEGA_MAX_ROOMS / ((uint32_t)cols * (uint32_t)rows));
    }

    out_mega->deck_count = decks;
    out_mega->cols = cols;
    out_mega->rows = rows;
    out_mega->room_w_m = rw;
    out_mega->room_d_m = rd;
    out_mega->deck_h_m = dh;
    out_mega->active_deck = 0;

    uint32_t seed = hash_str(bp->id);
    uint32_t idx = 0;
    double half_total_w = 0.5 * (double)cols * (double)rw;
    double half_total_d = 0.5 * (double)rows * (double)rd;

    for (uint16_t d = 0; d < decks; ++d) {
        for (int gz = 0; gz < (int)rows; ++gz) {
            for (int gx = 0; gx < (int)cols; ++gx) {
                RtsMegaRoomSpec *rm = &out_mega->rooms[idx];
                rm->room_index = (uint16_t)idx;
                rm->deck_index = d;
                rm->grid_x = (int16_t)gx;
                rm->grid_z = (int16_t)gz;
                rm->kind = pick_room_kind(bp, d, decks, gx, gz, (int)cols, (int)rows, seed);

                /* Orthogonal grid doorways guarantee 100% connectivity across every room on the deck */
                uint8_t dm = 0u;
                if (gz > 0) dm |= RTS_DOOR_N;
                if (gx + 1 < (int)cols) dm |= RTS_DOOR_E;
                if (gz + 1 < (int)rows) dm |= RTS_DOOR_S;
                if (gx > 0) dm |= RTS_DOOR_W;
                rm->door_mask = dm;

                double x0 = -half_total_w + (double)gx * (double)rw;
                double z0 = -half_total_d + (double)gz * (double)rd;
                rm->local_bounds.min = rts_v3d(x0, 0.0, z0);
                rm->local_bounds.max = rts_v3d(x0 + (double)rw, (double)dh, z0 + (double)rd);

                snprintf(rm->name, sizeof(rm->name), "D%u-%s %c%d",
                         (unsigned)(d + 1u), rts_room_kind_name(rm->kind),
                         (char)('A' + (gx % 26)), gz + 1);
                idx++;
            }
        }
    }
    out_mega->total_rooms = idx;
    return out_mega->total_rooms > 0u;
}

bool rts_mega_verify_connectivity(const RtsMegaInterior *mega, uint32_t *out_reachable_total) {
    if (!mega || mega->total_rooms == 0u || mega->cols == 0u || mega->rows == 0u) {
        return false;
    }
    uint32_t per_deck = (uint32_t)mega->cols * (uint32_t)mega->rows;
    uint32_t total_reached = 0u;
    uint8_t visited[64];
    uint16_t queue[64];

    for (uint16_t d = 0; d < mega->deck_count; ++d) {
        uint32_t base = (uint32_t)d * per_deck;
        if (per_deck > 64u || base + per_deck > mega->total_rooms) {
            return false;
        }
        memset(visited, 0, sizeof(visited));
        uint32_t qh = 0, qt = 0;
        uint16_t start = (uint16_t)((mega->rows / 2) * mega->cols + (mega->cols / 2));
        visited[start] = 1;
        queue[qt++] = start;

        while (qh < qt) {
            uint16_t cur = queue[qh++];
            const RtsMegaRoomSpec *rm = &mega->rooms[base + cur];
            int gx = rm->grid_x;
            int gz = rm->grid_z;
            if ((rm->door_mask & RTS_DOOR_N) && gz > 0) {
                uint16_t nb = (uint16_t)((gz - 1) * mega->cols + gx);
                if (!visited[nb]) { visited[nb] = 1; queue[qt++] = nb; }
            }
            if ((rm->door_mask & RTS_DOOR_E) && gx + 1 < (int)mega->cols) {
                uint16_t nb = (uint16_t)(gz * mega->cols + (gx + 1));
                if (!visited[nb]) { visited[nb] = 1; queue[qt++] = nb; }
            }
            if ((rm->door_mask & RTS_DOOR_S) && gz + 1 < (int)mega->rows) {
                uint16_t nb = (uint16_t)((gz + 1) * mega->cols + gx);
                if (!visited[nb]) { visited[nb] = 1; queue[qt++] = nb; }
            }
            if ((rm->door_mask & RTS_DOOR_W) && gx > 0) {
                uint16_t nb = (uint16_t)(gz * mega->cols + (gx - 1));
                if (!visited[nb]) { visited[nb] = 1; queue[qt++] = nb; }
            }
        }
        if (qt != per_deck) {
            return false;
        }
        total_reached += qt;
    }
    if (out_reachable_total) {
        *out_reachable_total = total_reached;
    }
    return total_reached == mega->total_rooms;
}

uint16_t rts_mega_find_deck_with_kind(const RtsMegaInterior *mega, RtsRoomKind kind) {
    if (!mega) return 0;
    for (uint32_t i = 0; i < mega->total_rooms; ++i) {
        if (mega->rooms[i].kind == kind) {
            return mega->rooms[i].deck_index;
        }
    }
    return 0;
}

bool rts_mega_hydrate_deck(RtsMegaInterior *mega, RtsWorld *w, uint32_t entity_id, uint16_t deck_index, RtsVec3d *out_spawn_pos) {
    if (!mega || !w || deck_index >= mega->deck_count) {
        return false;
    }
    RtsEntity *ent = rts_world_get_entity(w, entity_id);
    if (!ent || ent->own_frame_id == RTS_FRAME_INVALID) {
        return false;
    }

    RtsFrameId fid = ent->own_frame_id;
    rts_interior_clear_frame_contents(&w->interiors, fid);
    mega->active_deck = deck_index;

    uint32_t per_deck = (uint32_t)mega->cols * (uint32_t)mega->rows;
    uint32_t base = (uint32_t)deck_index * per_deck;
    RtsVec3d spawn = rts_v3d(0.0, 1.75, 0.0);
    bool have_spawn = false;

    for (uint32_t k = 0; k < per_deck && base + k < mega->total_rooms; ++k) {
        const RtsMegaRoomSpec *spec = &mega->rooms[base + k];
        uint32_t rid = rts_interior_add_room(&w->interiors, fid, spec->name, spec->local_bounds,
                                             light_for_room_kind(spec->kind), !spec->breached);
        RtsInteriorRoom *rm = rts_interior_get_room(&w->interiors, rid);
        if (rm) {
            rm->kind = spec->kind;
            rm->door_mask = spec->door_mask;
            rm->deck_index = spec->deck_index;
            rm->grid_x = spec->grid_x;
            rm->grid_z = spec->grid_z;
            rm->damaged_pct = spec->damaged_pct;
            rm->breached = spec->breached;
        }

        double cx = 0.5 * (spec->local_bounds.min.x + spec->local_bounds.max.x);
        double cz = 0.5 * (spec->local_bounds.min.z + spec->local_bounds.max.z);

        /* Spawn Commander inside War Room/Bridge on Deck 0, or Turbolift Hub on other decks */
        if (deck_index == 0 && (spec->kind == RTS_ROOM_WAR_ROOM || spec->kind == RTS_ROOM_BRIDGE_CIC) && !have_spawn) {
            spawn = rts_v3d(cx, 1.75, cz + 2.4);
            have_spawn = true;
        } else if (deck_index > 0 && spec->kind == RTS_ROOM_TURBOLIFT && !have_spawn) {
            spawn = rts_v3d(cx, 1.75, cz + 1.8);
            have_spawn = true;
        }

        /* Place role-accurate interactive consoles inside matching rooms */
        if (spec->kind == RTS_ROOM_WAR_ROOM) {
            rts_interior_add_console(&w->interiors, fid, "Strategic 3D Holo-Globe", RTS_CONSOLE_HOLO_GLOBE,
                                     rts_v3d(cx, 1.1, cz - 1.2), 4.2, entity_id);
        } else if (spec->kind == RTS_ROOM_BRIDGE_CIC) {
            RtsConsoleType ct = (mega->construct_class == RTS_CLASS_AIRCRAFT || mega->construct_class == RTS_CLASS_GROUND_VEHICLE)
                                ? RTS_CONSOLE_PILOT_SEAT : RTS_CONSOLE_BRIDGE_HELM;
            rts_interior_add_console(&w->interiors, fid, ct == RTS_CONSOLE_PILOT_SEAT ? "Pilot Cockpit Helm" : "Bridge Command Helm",
                                     ct, rts_v3d(cx, 1.1, cz - 1.8), 3.6, entity_id);
        } else if (spec->kind == RTS_ROOM_TURBOLIFT) {
            rts_interior_add_console(&w->interiors, fid, "Deck Turbolift Control", RTS_CONSOLE_TURBOLIFT,
                                     rts_v3d(cx, 1.1, cz - 1.4), 3.6, entity_id);
        } else if (spec->kind == RTS_ROOM_HANGAR || spec->kind == RTS_ROOM_ENGINEERING) {
            if (k < 6u) {
                rts_interior_add_console(&w->interiors, fid,
                                         spec->kind == RTS_ROOM_HANGAR ? "Flight Deck Launch Terminal" : "Engineering Console",
                                         RTS_CONSOLE_FACTORY_TERMINAL, rts_v3d(cx, 1.1, cz - 1.4), 3.4, entity_id);
            }
        } else if (spec->kind == RTS_ROOM_ESCAPE_POD) {
            rts_interior_add_console(&w->interiors, fid, "Emergency Escape Pod Hatch", RTS_CONSOLE_ESCAPE_POD,
                                     rts_v3d(cx, 1.1, cz), 3.4, entity_id);
        } else if (spec->kind == RTS_ROOM_WEAPONS) {
            if (k < 6u) {
                rts_interior_add_console(&w->interiors, fid, "Plasma Battery Fire Control", RTS_CONSOLE_TURRET_GUNNER,
                                         rts_v3d(cx, 1.1, cz - 1.2), 3.2, entity_id);
            }
        } else if (spec->kind == RTS_ROOM_AIRLOCK) {
            RtsAabb pbox = {
                rts_v3d(cx - 1.8, 0.0, spec->local_bounds.max.z - 1.4),
                rts_v3d(cx + 1.8, 3.2, spec->local_bounds.max.z + 0.5)
            };
            rts_interior_add_portal(&w->interiors, fid, w->planet_frame_id, "Exterior Boarding Airlock",
                                    pbox, rts_v3d(ent->pos.x + 18.0, ent->pos.y + 1.75, ent->pos.z + 18.0), true);
        }
    }

    if (out_spawn_pos) {
        *out_spawn_pos = spawn;
    }
    return true;
}

bool rts_mega_switch_deck(RtsMegaInterior *mega, RtsWorld *w, uint16_t target_deck) {
    if (!mega || !w || target_deck >= mega->deck_count) {
        return false;
    }
    RtsVec3d spawn;
    if (!rts_mega_hydrate_deck(mega, w, mega->entity_id, target_deck, &spawn)) {
        return false;
    }
    RtsEntity *ent = rts_world_get_entity(w, mega->entity_id);
    if (ent) {
        w->commander.frame_id = ent->own_frame_id;
        w->commander.occupied_entity_id = ent->id;
        w->commander.local_pos = spawn;
        w->commander.local_vel = rts_v3d(0.0, 0.0, 0.0);
        w->commander.at_holo_globe = false;
    }
    return true;
}
