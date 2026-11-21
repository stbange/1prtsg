/* SPDX-License-Identifier: MIT */
#include "rts/hologlobe_ui.h"
#include "rts/interior_gen.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static RtsQuatd hologlobe_orientation(const RtsHoloGlobeState *hg) {
    /* Tilt North-Pole surface theater (+Y) forward toward Commander (+Z) plus user spin */
    RtsQuatd qy = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), hg->yaw_rad);
    RtsQuatd qx = rts_quat_from_axis_angle(rts_v3d(1.0, 0.0, 0.0), hg->pitch_rad + 1.08);
    return rts_quat_mul(qy, qx);
}

static double hologlobe_theater_mag(const RtsHoloGlobeState *hg) {
    return 9600.0 * rts_clampd(hg->zoom, 0.55, 3.2);
}

void rts_hologlobe_init(RtsHoloGlobeState *hg) {
    if (!hg) return;
    memset(hg, 0, sizeof(*hg));
    hg->yaw_rad = 0.0;
    hg->pitch_rad = 0.0;
    hg->zoom = 1.4;
    hg->mode = RTS_HOLO_MODE_SELECT;
    hg->target_orbit_alt_m = 400000.0;
    hg->target_inclination_rad = 0.25;
    snprintf(hg->place_blueprint_id, sizeof(hg->place_blueprint_id), "bldg_solar_geothermal");
    snprintf(hg->last_feedback, sizeof(hg->last_feedback), "HOLO-GLOBE ONLINE: CLICK BLIPS TO SELECT OR ISSUE ORDERS");
}

const char *rts_hologlobe_mode_name(RtsHoloOrderMode mode) {
    switch (mode) {
        case RTS_HOLO_MODE_SELECT:         return "SELECT";
        case RTS_HOLO_MODE_MOVE:           return "SURFACE MOVE";
        case RTS_HOLO_MODE_ATTACK:         return "ATTACK TARGET";
        case RTS_HOLO_MODE_ORBIT:          return "ESTABLISH ORBIT";
        case RTS_HOLO_MODE_HOHMANN:        return "HOHMANN TRANSFER";
        case RTS_HOLO_MODE_DOCK:           return "RENDEZVOUS & DOCK";
        case RTS_HOLO_MODE_REENTRY:        return "SHIELDED RE-ENTRY";
        case RTS_HOLO_MODE_BOARD:          return "LAUNCH BOARDING";
        case RTS_HOLO_MODE_PLACE_BUILDING: return "3D BASE PLACEMENT";
        default:                           return "COMMAND";
    }
}

double rts_hologlobe_view_altitude_km(const RtsHoloGlobeState *hg) {
    if (!hg) return 400.0;
    /* Smooth exponential zoom: zoom 0.55 -> 36,000km GEO, zoom 1.4 -> 400km LEO, zoom 3.2 -> 4km Coastal Theater */
    double t = rts_clampd((hg->zoom - 0.55) / (3.2 - 0.55), 0.0, 1.0);
    return 36000.0 * pow(4.0 / 36000.0, t);
}

void rts_hologlobe_orbit_drag(RtsHoloGlobeState *hg, double dx_rad, double dy_rad) {
    if (!hg) return;
    hg->yaw_rad += dx_rad;
    hg->pitch_rad = rts_clampd(hg->pitch_rad + dy_rad, -0.95, 0.95);
}

void rts_hologlobe_zoom(RtsHoloGlobeState *hg, double delta) {
    if (!hg) return;
    hg->zoom = rts_clampd(hg->zoom + delta, 0.55, 3.2);
}

RtsVec3d rts_hologlobe_world_to_globe(const RtsHoloGlobeState *hg, const RtsPlanetConfig *planet,
                                      RtsVec3d world_pos, RtsVec3d globe_center, double globe_radius) {
    if (!hg || !planet) return globe_center;
    double r = rts_v3d_len(world_pos);
    RtsVec3d u = (r > 1.0) ? rts_v3d_scale(world_pos, 1.0 / r) : rts_v3d(0.0, 1.0, 0.0);
    double alt = r - planet->radius_m;

    if (alt < 50000.0 && u.y > 0.990) {
        double mag = hologlobe_theater_mag(hg);
        double hx = u.x * mag;
        double hz = u.z * mag;
        double hsq = hx * hx + hz * hz;
        if (hsq > 0.72) {
            double sc = sqrt(0.72 / hsq);
            hx *= sc;
            hz *= sc;
            hsq = 0.72;
        }
        u = rts_v3d(hx, sqrt(fmax(0.0, 1.0 - hsq)), hz);
    }

    double gr = globe_radius * 1.04;
    if (alt >= 50000.0) {
        double norm_alt = rts_clampd(log10(1.0 + alt / 100000.0) / 2.6, 0.0, 1.0);
        gr = globe_radius * (1.24 + 0.34 * norm_alt);
    }

    RtsQuatd q = hologlobe_orientation(hg);
    return rts_v3d_add(globe_center, rts_v3d_scale(rts_quat_rotate(q, u), gr));
}

RtsVec3d rts_hologlobe_globe_to_world(const RtsHoloGlobeState *hg, const RtsPlanetConfig *planet,
                                      RtsVec3d hit_point, RtsVec3d globe_center, double globe_radius,
                                      bool orbital_layer) {
    (void)globe_radius;
    if (!hg || !planet) return rts_v3d(0.0, 6371000.0, 0.0);
    RtsVec3d n_view = rts_v3d_norm(rts_v3d_sub(hit_point, globe_center));
    if (rts_v3d_len_sq(n_view) < 0.25) n_view = rts_v3d(0.0, 0.0, 1.0);

    RtsQuatd q_inv = rts_quat_conj(hologlobe_orientation(hg));
    RtsVec3d u = rts_quat_rotate(q_inv, n_view);

    if (orbital_layer) {
        double alt = hg->target_orbit_alt_m > 150000.0 ? hg->target_orbit_alt_m : 400000.0;
        return rts_v3d_scale(rts_v3d_norm(u), planet->radius_m + alt);
    }

    double mag = hologlobe_theater_mag(hg);
    double wx = (u.x / mag) * planet->radius_m;
    double wz = (u.z / mag) * planet->radius_m;
    double r2 = planet->radius_m * planet->radius_m;
    double wy2 = r2 - wx * wx - wz * wz;
    double wy = (wy2 > 0.0) ? sqrt(wy2) : 0.0;
    return rts_v3d(wx, wy, wz);
}

bool rts_hologlobe_is_selected(const RtsHoloGlobeState *hg, uint32_t entity_id) {
    if (!hg || entity_id == 0u) return false;
    for (size_t i = 0; i < hg->selected_count; ++i) {
        if (hg->selected_ids[i] == entity_id) return true;
    }
    return false;
}

void rts_hologlobe_clear_selection(RtsHoloGlobeState *hg) {
    if (!hg) return;
    hg->selected_count = 0u;
}

bool rts_hologlobe_select_entity(RtsHoloGlobeState *hg, uint32_t entity_id, bool additive) {
    if (!hg || entity_id == 0u) return false;
    if (!additive) {
        hg->selected_count = 0u;
    } else if (rts_hologlobe_is_selected(hg, entity_id)) {
        return true;
    }
    if (hg->selected_count >= RTS_HOLO_MAX_SEL) return false;
    hg->selected_ids[hg->selected_count++] = entity_id;
    return true;
}

bool rts_hologlobe_raycast(RtsHoloGlobeState *hg, const RtsWorld *w, uint8_t faction_id,
                           RtsRay ray, RtsVec3d globe_center, double globe_radius) {
    if (!hg || !w) return false;
    hg->hovered_entity_id = 0u;
    hg->hovered_valid = false;

    RtsRay rn = {ray.origin, rts_v3d_norm(ray.dir)};
    double best_score = 1e9;
    uint32_t best_id = 0u;
    RtsVec3d best_world = rts_v3d(0.0, w->planet.radius_m, 0.0);

    /* 1. Test ray intersection against every visible construct blip (respecting spherical-horizon Fog of War) */
    double blip_r = fmax(0.055, globe_radius * 0.075);
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (e->faction_id != faction_id && !rts_sensors_is_target_detected(w, faction_id, e->id)) {
            continue;
        }
        RtsVec3d bp = rts_hologlobe_world_to_globe(hg, &w->planet, e->pos, globe_center, globe_radius);
        double t = 0.0;
        if (rts_ray_intersect_sphere(rn, bp, blip_r, &t)) {
            RtsVec3d oc = rts_v3d_sub(bp, rn.origin);
            double proj = rts_v3d_dot(oc, rn.dir);
            RtsVec3d closest = rts_v3d_add(rn.origin, rts_v3d_scale(rn.dir, proj));
            double perp_d = rts_v3d_dist(closest, bp);
            double score = perp_d + t * 0.002;
            if (score < best_score) {
                best_score = score;
                best_id = e->id;
                best_world = e->pos;
            }
        }
    }

    if (best_id != 0u) {
        hg->hovered_entity_id = best_id;
        hg->hovered_world_pos = best_world;
        hg->hovered_valid = true;
        return true;
    }

    /* 2. Test ray intersection directly against the 3D Holo-Globe sphere surface or orbital shell */
    const RtsConstructBlueprint *place_bp = (hg->mode == RTS_HOLO_MODE_PLACE_BUILDING)
        ? rts_world_find_blueprint(w, hg->place_blueprint_id) : NULL;
    bool orbital_target = (hg->mode == RTS_HOLO_MODE_ORBIT || hg->mode == RTS_HOLO_MODE_HOHMANN ||
                           (place_bp && place_bp->construct_class == RTS_CLASS_SPACE_STATION));
    double pick_r = orbital_target ? globe_radius * 1.28 : globe_radius * 1.02;
    double hit_t = 0.0;
    if (rts_ray_intersect_sphere(rn, globe_center, pick_r, &hit_t)) {
        RtsVec3d hit_pt = rts_v3d_add(rn.origin, rts_v3d_scale(rn.dir, hit_t));
        hg->hovered_world_pos = rts_hologlobe_globe_to_world(hg, &w->planet, hit_pt, globe_center, globe_radius, orbital_target);
        hg->hovered_valid = true;
        return true;
    }
    return false;
}

size_t rts_hologlobe_box_select(RtsHoloGlobeState *hg, const RtsWorld *w, uint8_t faction_id,
                                RtsVec3d globe_center, double globe_radius, RtsVec3d min_pt, RtsVec3d max_pt) {
    if (!hg || !w) return 0u;
    RtsAabb box = {
        rts_v3d(fmin(min_pt.x, max_pt.x), fmin(min_pt.y, max_pt.y), fmin(min_pt.z, max_pt.z)),
        rts_v3d(fmax(min_pt.x, max_pt.x), fmax(min_pt.y, max_pt.y), fmax(min_pt.z, max_pt.z))
    };
    hg->selected_count = 0u;
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active || e->faction_id != faction_id) continue;
        RtsVec3d gp = rts_hologlobe_world_to_globe(hg, &w->planet, e->pos, globe_center, globe_radius);
        if (rts_aabb_contains(box, gp)) {
            rts_hologlobe_select_entity(hg, e->id, true);
        }
    }
    snprintf(hg->last_feedback, sizeof(hg->last_feedback), "BOX-SELECTED %zu FRIENDLY CONSTRUCTS", hg->selected_count);
    return hg->selected_count;
}

bool rts_hologlobe_issue_order(RtsHoloGlobeState *hg, RtsWorld *w, RtsCommanderController *ctrl,
                               RtsBoardingSystem *boarding, uint8_t faction_id,
                               RtsHoloOrderMode order_mode, RtsVec3d target_pos, uint32_t target_entity_id) {
    if (!hg || !w) return false;
    if (hg->selected_count == 0u && order_mode != RTS_HOLO_MODE_BOARD) {
        snprintf(hg->last_feedback, sizeof(hg->last_feedback), "NO UNITS SELECTED - CLICK A FRIENDLY BLIP ON THE GLOBE FIRST");
        return false;
    }

    size_t issued = 0u;
    if (order_mode == RTS_HOLO_MODE_BOARD) {
        if (!boarding || target_entity_id == 0u) return false;
        const RtsEntity *tgt = rts_world_get_entity_const(w, target_entity_id);
        if (!tgt || tgt->faction_id == faction_id) return false;
        uint32_t op = rts_boarding_start(boarding, w, target_entity_id, faction_id, 120u, true);
        if (op > 0u) {
            snprintf(hg->last_feedback, sizeof(hg->last_feedback), "BOARDING PODS LAUNCHED -> %.40s!", tgt->name);
            return true;
        }
        return false;
    }

    for (size_t i = 0; i < hg->selected_count; ++i) {
        uint32_t eid = hg->selected_ids[i];
        RtsEntity *e = rts_world_get_entity(w, eid);
        if (!e || !e->active || e->faction_id != faction_id) continue;

        if (order_mode == RTS_HOLO_MODE_MOVE) {
            if (rts_world_issue_order(w, eid, RTS_ORDER_MOVE, target_pos, 0u, 0.0)) issued++;
        } else if (order_mode == RTS_HOLO_MODE_ATTACK) {
            if (target_entity_id != 0u && rts_world_issue_order(w, eid, RTS_ORDER_ATTACK, target_pos, target_entity_id, 0.0)) issued++;
        } else if (order_mode == RTS_HOLO_MODE_ORBIT) {
            double alt = hg->target_orbit_alt_m >= 150000.0 ? hg->target_orbit_alt_m : 400000.0;
            if (ctrl && rts_astrogator_start_ascent(ctrl, w, eid, alt)) {
                issued++;
            } else if (rts_world_issue_order(w, eid, RTS_ORDER_ORBIT, target_pos, 0u, alt)) {
                issued++;
            }
        } else if (order_mode == RTS_HOLO_MODE_HOHMANN) {
            double alt = hg->target_orbit_alt_m >= 400000.0 ? hg->target_orbit_alt_m : 35786000.0;
            const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, e->blueprint_id);
            if (bp && bp->can_orbit) {
                RtsOrbitState st = rts_orbit_circular_state(&w->planet, alt, hg->target_inclination_rad, 0.02 * (double)(i + 1u));
                e->pos = st.pos_m;
                e->vel = st.vel_mps;
                e->orbital_mode = true;
                issued++;
            }
        } else if (order_mode == RTS_HOLO_MODE_DOCK) {
            if (ctrl && target_entity_id != 0u && rts_astrogator_start_rendezvous(ctrl, w, eid, target_entity_id)) {
                issued++;
            }
        } else if (order_mode == RTS_HOLO_MODE_REENTRY) {
            if (ctrl && rts_astrogator_start_reentry(ctrl, w, eid)) {
                issued++;
            }
        }
    }

    if (issued > 0u) {
        snprintf(hg->last_feedback, sizeof(hg->last_feedback), "%s ORDER ISSUED TO %zu CONSTRUCT(S)",
                 rts_hologlobe_mode_name(order_mode), issued);
        return true;
    }
    snprintf(hg->last_feedback, sizeof(hg->last_feedback), "ORDER %.24s NOT APPLICABLE TO SELECTED UNIT(S)",
             rts_hologlobe_mode_name(order_mode));
    return false;
}

const RtsConstructBlueprint *rts_hologlobe_cycle_building(RtsHoloGlobeState *hg, const RtsWorld *w, int delta) {
    if (!hg || !w) return NULL;
    const RtsConstructBlueprint *buildables[64];
    size_t count = 0u;
    for (size_t i = 0; i < w->blueprint_count && count < 64u; ++i) {
        RtsConstructClass cls = w->blueprints[i].construct_class;
        if (cls == RTS_CLASS_BUILDING || cls == RTS_CLASS_SPACE_STATION) {
            buildables[count++] = &w->blueprints[i];
        }
    }
    if (count == 0u) return NULL;
    int idx = (hg->place_catalog_idx + delta) % (int)count;
    if (idx < 0) idx += (int)count;
    hg->place_catalog_idx = idx;
    snprintf(hg->place_blueprint_id, sizeof(hg->place_blueprint_id), "%s", buildables[idx]->id);
    hg->mode = RTS_HOLO_MODE_PLACE_BUILDING;
    snprintf(hg->last_feedback, sizeof(hg->last_feedback), "3D PLACEMENT GHOST: %.44s", buildables[idx]->name);
    return buildables[idx];
}

bool rts_hologlobe_validate_placement(RtsHoloGlobeState *hg, const RtsWorld *w, const RtsEconomySystem *eco,
                                      uint8_t faction_id, const char *blueprint_id, RtsVec3d world_pos) {
    if (!hg || !w || !blueprint_id) return false;
    hg->ghost_world_pos = world_pos;
    hg->ghost_valid = false;

    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, blueprint_id);
    if (!bp || (bp->construct_class != RTS_CLASS_BUILDING && bp->construct_class != RTS_CLASS_SPACE_STATION)) {
        snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "NOT A PLACEABLE BUILDING OR STATION");
        return false;
    }

    double ore_cost = 0.0, fuel_cost = 0.0, build_s = 0.0;
    rts_economy_blueprint_cost(bp, &ore_cost, &fuel_cost, &build_s);
    if (eco && faction_id < RTS_MAX_FACTIONS) {
        if (eco->ledgers[faction_id].ore < ore_cost || eco->ledgers[faction_id].fuel < fuel_cost) {
            snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "INSUFFICIENT RESOURCES (NEED %.0f ORE)", ore_cost);
            return false;
        }
    }

    double alt = rts_planet_altitude(&w->planet, world_pos);
    if (bp->construct_class == RTS_CLASS_SPACE_STATION && alt < 150000.0) {
        snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "SPACE STATIONS REQUIRE >= 150KM ORBIT");
        return false;
    }
    if (bp->construct_class == RTS_CLASS_BUILDING && alt > 5000.0) {
        snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "BUILDINGS MUST BE PLACED ON PLANETARY SURFACE");
        return false;
    }

    /* Check overlap clearance against existing constructs */
    double min_clear = fmax(22.0, (double)bp->dimensions_m.x * 0.45);
    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (rts_v3d_dist(e->pos, world_pos) < min_clear) {
            snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "FOOTPRINT OBSTRUCTED BY %.28s", e->name);
            return false;
        }
    }

    hg->ghost_valid = true;
    snprintf(hg->ghost_reason, sizeof(hg->ghost_reason), "VALID SITE (%.0f ORE | %.0f FUEL)", ore_cost, fuel_cost);
    return true;
}

uint32_t rts_hologlobe_confirm_placement(RtsHoloGlobeState *hg, RtsWorld *w, RtsEconomySystem *eco,
                                         uint8_t faction_id, const char *blueprint_id, RtsVec3d world_pos) {
    if (!rts_hologlobe_validate_placement(hg, w, eco, faction_id, blueprint_id, world_pos)) {
        snprintf(hg->last_feedback, sizeof(hg->last_feedback), "PLACEMENT BLOCKED: %.60s", hg->ghost_reason);
        return 0u;
    }
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, blueprint_id);
    double ore_cost = 0.0, fuel_cost = 0.0, build_s = 0.0;
    rts_economy_blueprint_cost(bp, &ore_cost, &fuel_cost, &build_s);
    if (eco && faction_id < RTS_MAX_FACTIONS) {
        eco->ledgers[faction_id].ore -= ore_cost;
        eco->ledgers[faction_id].fuel -= fuel_cost;
        w->resources_ore = eco->ledgers[faction_id].ore;
        w->resources_fuel = eco->ledgers[faction_id].fuel;
    }

    uint32_t id = rts_world_spawn_from_blueprint(w, bp->id, bp->name, faction_id, w->planet_frame_id, world_pos, true);
    if (id != 0u) {
        RtsEntity *e = rts_world_get_entity(w, id);
        if (e && bp->construct_class == RTS_CLASS_SPACE_STATION) {
            double alt = rts_planet_altitude(&w->planet, world_pos);
            RtsOrbitState st = rts_orbit_circular_state(&w->planet, alt, hg->target_inclination_rad, 0.1);
            e->vel = st.vel_mps;
            e->orbital_mode = true;
        }
        snprintf(hg->last_feedback, sizeof(hg->last_feedback), "DEPLOYED %.40s (#%u)!", bp->name, id);
    }
    return id;
}
