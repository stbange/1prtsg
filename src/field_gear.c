/* SPDX-License-Identifier: MIT */
#include "rts/field_gear.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

void rts_field_gear_init(RtsFieldGearState *fg) {
    if (!fg) return;
    memset(fg, 0, sizeof(*fg));
    fg->wrist_tab = RTS_WPAD_RADAR;
    fg->radial_hovered = RTS_RADIAL_ATTACK;
    snprintf(fg->last_status, sizeof(fg->last_status), "FIELD GEAR READY: [TAB] WRIST-PAD | [Z] OPTICS | [V] RADIAL WHEEL");
}

const char *rts_wrist_tab_name(RtsWristPadTab tab) {
    switch (tab) {
        case RTS_WPAD_RADAR:        return "TACTICAL RADAR";
        case RTS_WPAD_DECK_MAP:     return "DECK BLUEPRINT";
        case RTS_WPAD_FLEET_ROSTER: return "FLEET ROSTER";
        case RTS_WPAD_ECONOMY:      return "ECONOMY & QUEUE";
        default:                    return "TACTICAL PAD";
    }
}

const char *rts_radial_order_name(RtsRadialOrder ord) {
    switch (ord) {
        case RTS_RADIAL_ESCORT:   return "ESCORT";
        case RTS_RADIAL_MOVE:     return "MOVE HERE";
        case RTS_RADIAL_ATTACK:   return "ATTACK TARGET";
        case RTS_RADIAL_ORBIT:    return "LAUNCH ORBIT";
        case RTS_RADIAL_DOCK:     return "DOCK STATION";
        case RTS_RADIAL_BOARD:    return "BOARD SHIP";
        case RTS_RADIAL_EVACUATE: return "EVACUATE POD";
        default:                  return "ORDER";
    }
}

float rts_field_gear_fov_deg(const RtsFieldGearState *fg) {
    if (!fg) return 62.0f;
    if (fg->optics_level == 1) return 20.0f;
    if (fg->optics_level >= 2) return 7.5f;
    return 62.0f;
}

int rts_field_gear_optics_zoom_x(const RtsFieldGearState *fg) {
    if (!fg) return 1;
    if (fg->optics_level == 1) return 4;
    if (fg->optics_level >= 2) return 12;
    return 1;
}

void rts_field_gear_toggle_wrist_pad(RtsFieldGearState *fg, RtsWorld *w) {
    if (!fg) return;
    fg->wrist_pad_open = !fg->wrist_pad_open;
    if (w) w->commander.wrist_pad_open = fg->wrist_pad_open;
    snprintf(fg->last_status, sizeof(fg->last_status), "TACTICAL WRIST-PAD %s [%s]",
             fg->wrist_pad_open ? "OPEN" : "STOWED", rts_wrist_tab_name(fg->wrist_tab));
}

void rts_field_gear_cycle_wrist_tab(RtsFieldGearState *fg, int delta) {
    if (!fg) return;
    int next = ((int)fg->wrist_tab + delta) % (int)RTS_WPAD_TAB_COUNT;
    if (next < 0) next += (int)RTS_WPAD_TAB_COUNT;
    fg->wrist_tab = (RtsWristPadTab)next;
    snprintf(fg->last_status, sizeof(fg->last_status), "WRIST-PAD TAB: %s", rts_wrist_tab_name(fg->wrist_tab));
}

int rts_field_gear_cycle_optics(RtsFieldGearState *fg) {
    if (!fg) return 1;
    fg->optics_level = (fg->optics_level + 1) % 3;
    int zx = rts_field_gear_optics_zoom_x(fg);
    snprintf(fg->last_status, sizeof(fg->last_status),
             zx == 1 ? "OPTICAL BINOCULARS STOWED (1X STANDARD VISOR)"
                     : "OPTICAL BINOCULARS & LASER DESIGNATOR ACTIVE (%dX ZOOM - LMB TO PAINT TARGET)", zx);
    return zx;
}

void rts_field_gear_toggle_radial(RtsFieldGearState *fg) {
    if (!fg) return;
    fg->radial_open = !fg->radial_open;
    snprintf(fg->last_status, sizeof(fg->last_status),
             fg->radial_open ? "RADIAL COMMAND WHEEL OPEN [1-7 OR CLICK]: ESCORT/MOVE/ATTACK/ORBIT/DOCK/BOARD/EVAC"
                             : "RADIAL COMMAND WHEEL CLOSED");
}

size_t rts_field_gear_scan_radar(const RtsWorld *w, uint8_t faction_id, uint32_t hq_entity_id,
                                 RtsVec3d observer_world_pos, double radar_range_m,
                                 RtsRadarContact *out_contacts, size_t max_contacts, bool *out_comms_ok) {
    if (!w || !out_contacts || max_contacts == 0u || radar_range_m <= 1.0) return 0u;
    size_t count = 0u;
    bool any_link = false;

    for (size_t i = 0; i < w->entity_count && count < max_contacts; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (e->faction_id != faction_id && !rts_sensors_is_target_detected(w, faction_id, e->id)) {
            continue;
        }
        RtsVec3d d = rts_v3d_sub(e->pos, observer_world_pos);
        double rx = d.x / radar_range_m;
        double rz = d.z / radar_range_m;
        double mag = sqrt(rx * rx + rz * rz);
        if (mag > 0.95) {
            rx = (rx / mag) * 0.95;
            rz = (rz / mag) * 0.95;
        }
        bool linked = (hq_entity_id != 0u && e->faction_id == faction_id)
            ? rts_comms_is_entity_linked(w, hq_entity_id, e->id) : (e->faction_id == faction_id);
        if (linked) any_link = true;

        RtsRadarContact *c = &out_contacts[count++];
        c->entity_id = e->id;
        c->faction_id = e->faction_id;
        c->rel_x_norm = (float)rx;
        c->rel_z_norm = (float)rz;
        c->orbital = e->orbital_mode;
        c->comms_linked = linked;
        snprintf(c->label, sizeof(c->label), "%.28s", e->name);
    }
    if (out_comms_ok) *out_comms_ok = any_link;
    return count;
}

bool rts_field_gear_paint_target(RtsFieldGearState *fg, const RtsWorld *w, uint8_t faction_id, RtsRay aim_ray) {
    if (!fg || !w) return false;
    RtsRay rn = {aim_ray.origin, rts_v3d_norm(aim_ray.dir)};
    double best_t = 1e9;
    const RtsEntity *best_e = NULL;

    for (size_t i = 0; i < w->entity_count; ++i) {
        const RtsEntity *e = &w->entities[i];
        if (!e->active) continue;
        if (e->id == w->commander.occupied_entity_id) continue;
        if (e->faction_id != faction_id && !rts_sensors_is_target_detected(w, faction_id, e->id)) continue;
        double hit_r = fmax(8.0, (double)fmaxf(e->dimensions_m.x, e->dimensions_m.z) * 0.6);
        double t = 0.0;
        if (rts_ray_intersect_sphere(rn, e->pos, hit_r, &t) && t > 1.0 && t < best_t) {
            best_t = t;
            best_e = e;
        }
    }

    if (best_e) {
        fg->paint_active = true;
        fg->painted_entity_id = best_e->id;
        fg->painted_world_pos = best_e->pos;
        snprintf(fg->painted_label, sizeof(fg->painted_label), "LOCK: %.44s", best_e->name);
        snprintf(fg->last_status, sizeof(fg->last_status), "LASER DESIGNATOR LOCKED ON %.44s (%.0fM)", best_e->name, best_t);
        return true;
    }

    /* Fallback: paint point on planetary horizon plane / surface sphere */
    double pt = 0.0;
    if (rts_ray_intersect_sphere(rn, rts_v3d(0.0, 0.0, 0.0), w->planet.radius_m, &pt) && pt > 1.0 && pt < 40000.0) {
        fg->paint_active = true;
        fg->painted_entity_id = 0u;
        fg->painted_world_pos = rts_v3d_add(rn.origin, rts_v3d_scale(rn.dir, pt));
        snprintf(fg->painted_label, sizeof(fg->painted_label), "LZ / STRIKE COORD (%.0fM)", pt);
        snprintf(fg->last_status, sizeof(fg->last_status), "LASER DESIGNATED SURFACE LZ / ARTILLERY COORD AT %.0fM", pt);
        return true;
    }

    /* If looking toward open space/horizon, project a 180m tactical waypoint ahead */
    fg->paint_active = true;
    fg->painted_entity_id = 0u;
    fg->painted_world_pos = rts_v3d_add(rn.origin, rts_v3d_scale(rn.dir, 180.0));
    snprintf(fg->painted_label, sizeof(fg->painted_label), "VECTOR BEACON (180M)");
    snprintf(fg->last_status, sizeof(fg->last_status), "LASER VECTOR BEACON SET AT 180M");
    return true;
}

size_t rts_field_gear_designate_strike(RtsFieldGearState *fg, RtsWorld *w, uint8_t faction_id) {
    if (!fg || !w || !fg->paint_active) return 0u;
    size_t tasked = 0u;
    for (size_t i = 0; i < w->entity_count; ++i) {
        RtsEntity *e = &w->entities[i];
        if (!e->active || e->faction_id != faction_id || e->max_speed_mps <= 0.1f) continue;
        if (fg->painted_entity_id != 0u) {
            const RtsEntity *tgt = rts_world_get_entity_const(w, fg->painted_entity_id);
            if (tgt && tgt->faction_id != faction_id) {
                if (rts_world_issue_order(w, e->id, RTS_ORDER_ATTACK, fg->painted_world_pos, fg->painted_entity_id, 0.0)) {
                    tasked++;
                }
                continue;
            }
        }
        if (rts_world_issue_order(w, e->id, RTS_ORDER_MOVE, fg->painted_world_pos, 0u, 0.0)) {
            tasked++;
        }
    }
    snprintf(fg->last_status, sizeof(fg->last_status), "LASER DESIGNATION TRANSMITTED -> %zu UNIT(S) TASKED (%s)",
             tasked, fg->painted_label);
    return tasked;
}

bool rts_field_gear_exec_radial(RtsFieldGearState *fg, RtsWorld *w, RtsCommanderController *ctrl,
                                RtsBoardingSystem *boarding, RtsHoloGlobeState *hg,
                                uint8_t faction_id, RtsRadialOrder ord, RtsVec3d commander_world_pos) {
    if (!fg || !w) return false;
    fg->radial_hovered = ord;
    fg->radial_open = false;

    if (ord == RTS_RADIAL_EVACUATE) {
        bool ok = rts_world_commander_launch_escape_pod(w);
        snprintf(fg->last_status, sizeof(fg->last_status),
                 ok ? "RADIAL [EVACUATE]: EMERGENCY ESCAPE POD LAUNCHED!"
                    : "RADIAL [EVACUATE]: NO ESCAPE POD ON CURRENT CONSTRUCT");
        return ok;
    }

    if (ord == RTS_RADIAL_ESCORT) {
        size_t n = 0u;
        for (size_t i = 0; i < w->entity_count; ++i) {
            RtsEntity *e = &w->entities[i];
            if (!e->active || e->faction_id != faction_id || e->max_speed_mps <= 0.1f || e->orbital_mode) continue;
            RtsVec3d rally = rts_v3d(commander_world_pos.x + 22.0 * (double)(n + 1u), commander_world_pos.y, commander_world_pos.z);
            if (rts_world_issue_order(w, e->id, RTS_ORDER_MOVE, rally, 0u, 0.0)) n++;
        }
        snprintf(fg->last_status, sizeof(fg->last_status), "RADIAL [ESCORT]: %zu FRIENDLY UNIT(S) RALLYING TO COMMANDER", n);
        return n > 0u;
    }

    /* Ensure at least one controllable friendly unit is selected for Holo-Globe order dispatch */
    if (hg && hg->selected_count == 0u) {
        for (size_t i = 0; i < w->entity_count; ++i) {
            const RtsEntity *e = &w->entities[i];
            if (e->active && e->faction_id == faction_id && e->max_speed_mps > 0.1f) {
                rts_hologlobe_select_entity(hg, e->id, true);
            }
        }
    }

    RtsVec3d tpos = fg->paint_active ? fg->painted_world_pos : rts_v3d(commander_world_pos.x + 80.0, commander_world_pos.y, commander_world_pos.z - 60.0);
    uint32_t tid = fg->paint_active ? fg->painted_entity_id : 0u;

    if (ord == RTS_RADIAL_ATTACK || ord == RTS_RADIAL_BOARD) {
        if (tid == 0u) {
            for (size_t i = 0; i < w->entity_count; ++i) {
                const RtsEntity *e = &w->entities[i];
                if (!e->active || e->faction_id == faction_id) continue;
                if (ord == RTS_RADIAL_BOARD && !e->orbital_mode) continue;
                tid = e->id;
                tpos = e->pos;
                break;
            }
        }
    } else if (ord == RTS_RADIAL_DOCK && tid == 0u) {
        for (size_t i = 0; i < w->entity_count; ++i) {
            const RtsEntity *e = &w->entities[i];
            if (e->active && e->faction_id == faction_id && e->orbital_mode && e->max_speed_mps <= 0.1f) {
                tid = e->id;
                tpos = e->pos;
                break;
            }
        }
    }

    RtsHoloOrderMode hm = RTS_HOLO_MODE_MOVE;
    if (ord == RTS_RADIAL_ATTACK) hm = RTS_HOLO_MODE_ATTACK;
    else if (ord == RTS_RADIAL_ORBIT) hm = RTS_HOLO_MODE_ORBIT;
    else if (ord == RTS_RADIAL_DOCK) hm = RTS_HOLO_MODE_DOCK;
    else if (ord == RTS_RADIAL_BOARD) hm = RTS_HOLO_MODE_BOARD;

    bool ok = hg ? rts_hologlobe_issue_order(hg, w, ctrl, boarding, faction_id, hm, tpos, tid) : false;
    snprintf(fg->last_status, sizeof(fg->last_status), "RADIAL [%s]: %.96s",
             rts_radial_order_name(ord), hg ? hg->last_feedback : "UNAVAILABLE");
    return ok;
}

static RtsConstructClass infer_facility_output_class(const char *bp_id) {
    if (!bp_id) return RTS_CLASS_GROUND_VEHICLE;
    if (strstr(bp_id, "shipyard") || strstr(bp_id, "drydock") || strstr(bp_id, "spaceport") || strstr(bp_id, "citadel")) {
        return RTS_CLASS_SPACECRAFT;
    }
    if (strstr(bp_id, "carrier") || strstr(bp_id, "hangar") || strstr(bp_id, "airbase")) {
        return RTS_CLASS_AIRCRAFT;
    }
    if (strstr(bp_id, "naval") || strstr(bp_id, "dock") || strstr(bp_id, "pier")) {
        return RTS_CLASS_NAVAL_SHIP;
    }
    return RTS_CLASS_GROUND_VEHICLE;
}

bool rts_facility_open_for_entity(RtsFieldGearState *fg, const RtsWorld *w, uint32_t facility_entity_id) {
    if (!fg || !w) return false;
    const RtsEntity *ent = rts_world_get_entity_const(w, facility_entity_id);
    if (!ent || !ent->active) return false;

    RtsConstructClass out_cls = infer_facility_output_class(ent->blueprint_id);
    fg->facility_entity_id = ent->id;
    snprintf(fg->facility_name, sizeof(fg->facility_name), "%.56s", ent->name);
    fg->facility_output_class = out_cls;
    fg->facility_item_count = 0u;
    fg->facility_selected_idx = 0;

    for (size_t i = 0; i < w->blueprint_count && fg->facility_item_count < RTS_FACILITY_MAX_ITEMS; ++i) {
        if (w->blueprints[i].construct_class == out_cls) {
            snprintf(fg->facility_items[fg->facility_item_count++], 64u, "%s", w->blueprints[i].id);
        }
    }
    fg->facility_open = (fg->facility_item_count > 0u);
    if (fg->facility_open) {
        snprintf(fg->last_status, sizeof(fg->last_status), "FACILITY TERMINAL ONLINE: %.38s (%zu BLUEPRINTS - PRESS 1-6 TO BUILD)",
                 ent->name, fg->facility_item_count);
    }
    return fg->facility_open;
}

void rts_facility_close(RtsFieldGearState *fg) {
    if (!fg) return;
    fg->facility_open = false;
}

uint32_t rts_facility_queue_index(RtsFieldGearState *fg, RtsEconomySystem *eco, const RtsWorld *w, int item_idx) {
    if (!fg || !eco || !w || !fg->facility_open) return 0u;
    if (item_idx < 0 || (size_t)item_idx >= fg->facility_item_count) return 0u;
    fg->facility_selected_idx = item_idx;
    const char *bp_id = fg->facility_items[item_idx];
    uint32_t jid = rts_economy_queue_build(eco, w, fg->facility_entity_id, bp_id);
    if (jid > 0u) {
        const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, bp_id);
        snprintf(fg->last_status, sizeof(fg->last_status),
                 "1:1 GANTRY ASSEMBLY STARTED: %.44s (JOB #%u)", bp ? bp->name : bp_id, jid);
    } else {
        snprintf(fg->last_status, sizeof(fg->last_status), "PRODUCTION QUEUE REJECTED (INSUFFICIENT ORE/FUEL OR QUEUE FULL)");
    }
    return jid;
}
