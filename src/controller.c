/* SPDX-License-Identifier: MIT */
#include "rts/controller.h"
#include <math.h>
#include <string.h>

void rts_controller_init(RtsCommanderController *ctrl) {
    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->mode = RTS_CMD_ON_FOOT;
    ctrl->walk_speed_mps = 6.5;
}

bool rts_controller_engage_piloting(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id) {
    if (!ctrl || !w) return false;
    RtsEntity *e = rts_world_get_entity(w, craft_id);
    if (!e || e->max_speed_mps <= 0.0f) return false;
    ctrl->mode = RTS_CMD_PILOTING;
    ctrl->piloted_entity_id = craft_id;
    ctrl->pilot_throttle = 0.0;
    w->commander.occupied_entity_id = craft_id;
    if (e->own_frame_id != RTS_FRAME_INVALID) {
        w->commander.frame_id = e->own_frame_id;
    }
    return true;
}

void rts_controller_release_piloting(RtsCommanderController *ctrl) {
    if (!ctrl) return;
    ctrl->mode = RTS_CMD_ON_FOOT;
    ctrl->piloted_entity_id = 0u;
    ctrl->pilot_throttle = 0.0;
}

void rts_controller_step(RtsCommanderController *ctrl, RtsWorld *w, const RtsInputIntent *intent, double dt_s) {
    if (!ctrl || !w || !intent || !w->commander.alive || dt_s <= 0.0) return;

    if (intent->toggle_pad_pressed) {
        w->commander.wrist_pad_open = !w->commander.wrist_pad_open;
    }
    if (intent->mag_boot_pressed) {
        w->commander.mag_boots = !w->commander.mag_boots;
    }

    w->commander.yaw_rad += (double)intent->look_dx;
    w->commander.pitch_rad = rts_clampd(w->commander.pitch_rad + (double)intent->look_dy, -1.4, 1.4);

    if (ctrl->mode == RTS_CMD_PILOTING) {
        if (intent->interact_pressed) {
            rts_controller_release_piloting(ctrl);
            return;
        }
        RtsEntity *craft = rts_world_get_entity(w, ctrl->piloted_entity_id);
        if (!craft) {
            rts_controller_release_piloting(ctrl);
            return;
        }
        ctrl->pilot_throttle = rts_clampd(ctrl->pilot_throttle + (double)intent->move_y * dt_s, -0.5, 1.0);
        double speed = (double)rts_world_effective_speed(craft) * ctrl->pilot_throttle;
        RtsVec3d fwd = rts_v3d(sin(w->commander.yaw_rad), sin(w->commander.pitch_rad), -cos(w->commander.yaw_rad));
        fwd = rts_v3d_norm(fwd);
        craft->vel = rts_v3d_scale(fwd, speed);
        craft->pos = rts_v3d_add(craft->pos, rts_v3d_scale(craft->vel, dt_s));
        craft->rot = rts_quat_from_axis_angle(rts_v3d(0, 1, 0), w->commander.yaw_rad);
        return;
    }

    if (ctrl->mode == RTS_CMD_AT_HOLO_GLOBE || ctrl->mode == RTS_CMD_AT_FACTORY) {
        if (intent->interact_pressed) {
            ctrl->mode = RTS_CMD_ON_FOOT;
            w->commander.at_holo_globe = false;
            ctrl->active_console_id = 0u;
        }
        return;
    }

    /* RTS_CMD_ON_FOOT locomotion inside current SpatialFrame with snappy non-linear acceleration/damping */
    double cy = cos(w->commander.yaw_rad);
    double sy = sin(w->commander.yaw_rad);
    RtsVec3d fwd = rts_v3d(sy, 0.0, -cy);
    RtsVec3d right = rts_v3d(cy, 0.0, sy);
    RtsVec3d move = rts_v3d_add(rts_v3d_scale(right, (double)intent->move_x), rts_v3d_scale(fwd, (double)intent->move_y));
    double mlen = rts_v3d_len(move);
    if (mlen > 1.0) {
        move = rts_v3d_scale(move, 1.0 / mlen);
    }
    RtsVec3d target_vel = rts_v3d_scale(move, ctrl->walk_speed_mps);
    double damp_rate = (mlen > 0.05) ? 20.0 : 15.0;
    double alpha = 1.0 - exp(-damp_rate * dt_s);
    w->commander.local_vel.x += (target_vel.x - w->commander.local_vel.x) * alpha;
    w->commander.local_vel.z += (target_vel.z - w->commander.local_vel.z) * alpha;

    RtsVec3d next_pos = rts_v3d_add(w->commander.local_pos, rts_v3d_scale(w->commander.local_vel, dt_s));
    const RtsInteriorRoom *cur_room = rts_interior_find_room_at(&w->interiors, w->commander.frame_id, w->commander.local_pos);
    if (cur_room && !rts_aabb_contains(cur_room->local_bounds, next_pos)) {
        /* Clamp cleanly inside current pressurized room unless stepping into a portal */
        next_pos.x = rts_clampd(next_pos.x, cur_room->local_bounds.min.x + 0.3, cur_room->local_bounds.max.x - 0.3);
        next_pos.z = rts_clampd(next_pos.z, cur_room->local_bounds.min.z + 0.3, cur_room->local_bounds.max.z - 0.3);
    }
    w->commander.local_pos = next_pos;
    rts_interior_try_portal_transfer(&w->interiors, &w->commander.frame_id, &w->commander.local_pos, &w->commander.local_vel);

    if (intent->interact_pressed) {
        ctrl->interact_buffer_s = 0.12; /* 120ms input buffer for responsive console interaction */
    }
    if (ctrl->interact_buffer_s > 0.0) {
        const RtsConsoleStation *con = rts_interior_find_nearest_console(&w->interiors, w->commander.frame_id, w->commander.local_pos);
        if (con) {
            ctrl->interact_buffer_s = 0.0;
            ctrl->active_console_id = con->id;
            if (con->type == RTS_CONSOLE_HOLO_GLOBE) {
                ctrl->mode = RTS_CMD_AT_HOLO_GLOBE;
                w->commander.at_holo_globe = true;
            } else if (con->type == RTS_CONSOLE_FACTORY_TERMINAL) {
                ctrl->mode = RTS_CMD_AT_FACTORY;
            } else if (con->type == RTS_CONSOLE_PILOT_SEAT || con->type == RTS_CONSOLE_BRIDGE_HELM) {
                rts_controller_engage_piloting(ctrl, w, con->owner_entity_id);
            } else if (con->type == RTS_CONSOLE_ESCAPE_POD) {
                rts_world_commander_launch_escape_pod(w);
            }
        } else {
            ctrl->interact_buffer_s = fmax(0.0, ctrl->interact_buffer_s - dt_s);
        }
    }
}

bool rts_astrogator_start_ascent(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id, double target_alt_m) {
    if (!ctrl || !w || target_alt_m < 150000.0) return false;
    RtsEntity *e = rts_world_get_entity(w, craft_id);
    if (!e) return false;
    const RtsConstructBlueprint *bp = rts_world_find_blueprint(w, e->blueprint_id);
    if (!bp || !bp->can_orbit) return false;
    ctrl->astrogator.craft_entity_id = craft_id;
    ctrl->astrogator.target_altitude_m = target_alt_m;
    ctrl->astrogator.phase = RTS_AUTO_ASCENT;
    ctrl->astrogator.docked = false;
    ctrl->astrogator.peak_reentry_heat = 0.0;
    e->orbital_mode = false;
    return true;
}

bool rts_astrogator_start_rendezvous(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id, uint32_t station_id) {
    if (!ctrl || !w) return false;
    if (!rts_world_get_entity(w, craft_id) || !rts_world_get_entity(w, station_id)) return false;
    ctrl->astrogator.craft_entity_id = craft_id;
    ctrl->astrogator.target_station_id = station_id;
    ctrl->astrogator.phase = RTS_AUTO_RENDEZVOUS;
    ctrl->astrogator.docked = false;
    return true;
}

bool rts_astrogator_start_reentry(RtsCommanderController *ctrl, RtsWorld *w, uint32_t craft_id) {
    if (!ctrl || !w) return false;
    RtsEntity *e = rts_world_get_entity(w, craft_id);
    if (!e) return false;
    ctrl->astrogator.craft_entity_id = craft_id;
    ctrl->astrogator.phase = RTS_AUTO_REENTRY;
    ctrl->astrogator.peak_reentry_heat = 0.0;
    e->orbital_mode = false;
    return true;
}

RtsAutopilotPhase rts_astrogator_step(RtsCommanderController *ctrl, RtsWorld *w, double dt_s) {
    if (!ctrl || !w || dt_s <= 0.0 || ctrl->astrogator.phase == RTS_AUTO_IDLE) {
        return RTS_AUTO_IDLE;
    }
    RtsEntity *e = rts_world_get_entity(w, ctrl->astrogator.craft_entity_id);
    if (!e) {
        ctrl->astrogator.phase = RTS_AUTO_IDLE;
        return RTS_AUTO_IDLE;
    }
    double alt = rts_planet_altitude(&w->planet, e->pos);
    RtsVec3d radial = rts_v3d_norm(e->pos);
    if (rts_v3d_len_sq(radial) < 0.5) radial = rts_v3d(0.0, 1.0, 0.0);

    if (ctrl->astrogator.phase == RTS_AUTO_ASCENT) {
        /* Space-opera torch-drive gravity turn ascent */
        double climb_rate = 6500.0; /* m/s high-thrust ascent profile */
        e->pos = rts_v3d_add(e->pos, rts_v3d_scale(radial, climb_rate * dt_s));
        alt = rts_planet_altitude(&w->planet, e->pos);
        if (alt >= ctrl->astrogator.target_altitude_m) {
            RtsOrbitState st = rts_orbit_circular_state(&w->planet, ctrl->astrogator.target_altitude_m, 0.25, 0.0);
            e->pos = st.pos_m;
            e->vel = st.vel_mps;
            e->orbital_mode = true;
            ctrl->astrogator.phase = RTS_AUTO_CIRCULARIZED;
        }
    } else if (ctrl->astrogator.phase == RTS_AUTO_RENDEZVOUS) {
        RtsEntity *stn = rts_world_get_entity(w, ctrl->astrogator.target_station_id);
        if (!stn) {
            ctrl->astrogator.phase = RTS_AUTO_IDLE;
            return RTS_AUTO_IDLE;
        }
        RtsVec3d diff = rts_v3d_sub(stn->pos, e->pos);
        double d = rts_v3d_len(diff);
        double step = 5000.0 * dt_s;
        if (d <= step || d < 25.0) {
            e->pos = stn->pos;
            e->vel = stn->vel;
            e->orbital_mode = stn->orbital_mode;
            ctrl->astrogator.docked = true;
            ctrl->astrogator.phase = RTS_AUTO_IDLE;
            if (e->own_frame_id != RTS_FRAME_INVALID && stn->own_frame_id != RTS_FRAME_INVALID) {
                rts_interior_reparent_frame(&w->interiors, e->own_frame_id, stn->own_frame_id, rts_v3d(15.0, 0.0, 0.0), rts_quat_identity());
            }
        } else {
            e->pos = rts_v3d_add(e->pos, rts_v3d_scale(diff, step / d));
        }
    } else if (ctrl->astrogator.phase == RTS_AUTO_REENTRY) {
        /* Shielded atmospheric re-entry descent with aerobraking deceleration */
        double atmo_frac = rts_clampd(alt / 120000.0, 0.0, 1.0);
        double descent_speed = 1800.0 + 5200.0 * atmo_frac;
        double aero_speed = 1200.0 + 4800.0 * atmo_frac;
        double heat = rts_planet_reentry_heat_flux(&w->planet, alt, aero_speed);
        if (heat > ctrl->astrogator.peak_reentry_heat) {
            ctrl->astrogator.peak_reentry_heat = heat;
        }
        if (heat > 5.0) {
            double thermal_dmg = fmin(heat * 0.0025, 18.0) * dt_s;
            rts_world_apply_damage(w, e->id, (float)thermal_dmg, RTS_SUBSYS_SENSORS);
        }
        double next_alt = alt - descent_speed * dt_s;
        if (next_alt <= 10.0) {
            next_alt = 10.0;
            e->vel = rts_v3d(0.0, 0.0, 0.0);
            ctrl->astrogator.phase = RTS_AUTO_LANDED;
        }
        e->pos = rts_v3d_scale(radial, w->planet.radius_m + next_alt);
    }
    return ctrl->astrogator.phase;
}
