/* SPDX-License-Identifier: MIT */
#include "rts/vehicle_physics.h"
#include "rts/planet_mesh.h"
#include <math.h>
#include <string.h>

void rts_vehicle_physics_init(RtsVehiclePhysicsState *vp) {
    if (!vp) return;
    memset(vp, 0, sizeof(*vp));
    vp->hover_clearance_m = 1.4;
    vp->vtol_blend = 1.0;
    vp->time_compression = 1;
}

void rts_vehicle_physics_bind(RtsVehiclePhysicsState *vp, const RtsWorld *w, uint32_t entity_id) {
    if (!vp || !w) return;
    const RtsEntity *e = rts_world_get_entity_const(w, entity_id);
    if (!e) return;
    vp->entity_id = entity_id;
    vp->active_class = e->construct_class;
    vp->is_submarine = (e->construct_class == RTS_CLASS_NAVAL_SHIP &&
                        (strstr(e->blueprint_id, "sub") != NULL || strstr(e->name, "Submarine") != NULL));
    vp->ballast_depth_m = 0.0;
    vp->target_depth_m = 0.0;
    vp->vtol_blend = (e->construct_class == RTS_CLASS_AIRCRAFT) ? 1.0 : 0.0;
    vp->ramp_open = false;
    vp->reentry_heat_wm2 = 0.0;
    vp->reentry_glow = 0.0f;
}

bool rts_vehicle_clamp_cargo(RtsVehiclePhysicsState *vp, RtsWorld *w, uint32_t carrier_id, uint32_t cargo_id) {
    if (!vp || !w || carrier_id == 0u || cargo_id == 0u || carrier_id == cargo_id) return false;
    RtsEntity *carrier = rts_world_get_entity(w, carrier_id);
    RtsEntity *cargo = rts_world_get_entity(w, cargo_id);
    if (!carrier || !cargo) return false;
    if (carrier->construct_class != RTS_CLASS_AIRCRAFT && carrier->construct_class != RTS_CLASS_SPACECRAFT) return false;
    if (cargo->construct_class != RTS_CLASS_GROUND_VEHICLE) return false;
    if (carrier->faction_id != cargo->faction_id) return false;
    if (rts_v3d_dist(carrier->pos, cargo->pos) > 85.0) return false;

    vp->clamped_cargo_id = cargo_id;
    vp->ramp_open = false;
    cargo->pos = rts_v3d(carrier->pos.x, carrier->pos.y - 1.2, carrier->pos.z);
    cargo->vel = carrier->vel;
    cargo->rot = carrier->rot;
    cargo->order_type = RTS_ORDER_NONE;
    return true;
}

bool rts_vehicle_release_cargo(RtsVehiclePhysicsState *vp, RtsWorld *w) {
    if (!vp || !w || vp->clamped_cargo_id == 0u) return false;
    RtsEntity *carrier = rts_world_get_entity(w, vp->entity_id);
    RtsEntity *cargo = rts_world_get_entity(w, vp->clamped_cargo_id);
    if (!carrier || !cargo) {
        vp->clamped_cargo_id = 0u;
        return false;
    }
    double alt = rts_planet_altitude(&w->planet, carrier->pos);
    RtsVec3d up = rts_v3d_norm(carrier->pos);
    double ground_h = rts_planet_local_elevation(&w->planet, up);
    if (alt - ground_h > 45.0) return false; /* Too high to drop safely */

    RtsVec3d fwd = rts_quat_rotate(carrier->rot, rts_v3d(0.0, 0.0, 12.0));
    RtsVec3d drop_dir = rts_v3d_norm(rts_v3d_add(carrier->pos, fwd));
    double drop_h = rts_planet_local_elevation(&w->planet, drop_dir);
    cargo->pos = rts_v3d_scale(drop_dir, w->planet.radius_m + fmax(drop_h, w->planet.sea_level_m));
    cargo->vel = rts_v3d(0.0, 0.0, 0.0);
    vp->clamped_cargo_id = 0u;
    vp->ramp_open = true;
    return true;
}

int rts_time_compression_set(RtsVehiclePhysicsState *vp, int factor, bool combat_active) {
    if (!vp) return 1;
    if (combat_active) {
        vp->time_compression = 1;
        vp->time_warp_blocked_combat = true;
        return 1;
    }
    vp->time_warp_blocked_combat = false;
    if (factor >= 8) vp->time_compression = 8;
    else if (factor >= 4) vp->time_compression = 4;
    else if (factor >= 2) vp->time_compression = 2;
    else vp->time_compression = 1;
    return vp->time_compression;
}

int rts_time_compression_cycle(RtsVehiclePhysicsState *vp, bool combat_active) {
    if (!vp) return 1;
    int next = (vp->time_compression == 1) ? 2 : (vp->time_compression == 2 ? 4 : (vp->time_compression == 4 ? 8 : 1));
    return rts_time_compression_set(vp, next, combat_active);
}

void rts_vehicle_physics_step(RtsVehiclePhysicsState *vp, RtsWorld *w, const RtsInputIntent *intent, double dt_s) {
    if (!vp || !w || !intent || dt_s <= 0.0) return;
    RtsEntity *e = rts_world_get_entity(w, vp->entity_id);
    if (!e) return;

    double max_spd = (double)rts_world_effective_speed(e);
    if (max_spd < 4.0) max_spd = 12.0;

    RtsVec3d up = rts_v3d_norm(e->pos);
    if (rts_v3d_len_sq(up) < 1e-9) up = rts_v3d(0.0, 1.0, 0.0);
    double yaw = w->commander.yaw_rad;
    RtsVec3d fwd = rts_v3d(sin(yaw), 0.0, -cos(yaw));
    RtsVec3d right = rts_v3d(cos(yaw), 0.0, sin(yaw));

    if (e->construct_class == RTS_CLASS_GROUND_VEHICLE) {
        /* Hover-cushion suspension + surface normal slope alignment + independent turret aim */
        RtsVec3d desired = rts_v3d_add(rts_v3d_scale(fwd, intent->move_y * max_spd),
                                       rts_v3d_scale(right, intent->move_x * max_spd * 0.65));
        e->vel = rts_v3d_add(rts_v3d_scale(e->vel, 0.82), rts_v3d_scale(desired, 0.18));
        e->pos = rts_v3d_add(e->pos, rts_v3d_scale(e->vel, dt_s));

        RtsVec3d nu = rts_v3d_norm(e->pos);
        double ground_h = rts_planet_local_elevation(&w->planet, nu);
        if (ground_h < w->planet.sea_level_m) ground_h = w->planet.sea_level_m;
        e->pos = rts_v3d_scale(nu, w->planet.radius_m + ground_h + vp->hover_clearance_m);

        RtsVec3d sn = rts_planet_local_normal(&w->planet, nu, 6.0);
        vp->slope_pitch_rad = rts_clampd(rts_v3d_dot(sn, fwd) * 0.5, -0.45, 0.45);
        vp->slope_roll_rad = rts_clampd(rts_v3d_dot(sn, right) * 0.5, -0.45, 0.45);
        vp->turret_yaw_rad = w->commander.yaw_rad;
        vp->turret_pitch_rad = rts_clampd(w->commander.pitch_rad, -0.35, 0.65);

        RtsQuatd qy = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), -yaw);
        RtsQuatd qp = rts_quat_from_axis_angle(rts_v3d(1.0, 0.0, 0.0), vp->slope_pitch_rad);
        e->rot = rts_quat_norm(rts_quat_mul(qy, qp));
    } else if (e->construct_class == RTS_CLASS_NAVAL_SHIP) {
        /* Ocean buoyancy, dynamic wave roll/pitch, wake foam & Submarine ballast depth control */
        RtsVec3d desired = rts_v3d_add(rts_v3d_scale(fwd, intent->move_y * max_spd),
                                       rts_v3d_scale(right, intent->move_x * max_spd * 0.4));
        e->vel = rts_v3d_add(rts_v3d_scale(e->vel, 0.90), rts_v3d_scale(desired, 0.10));
        e->pos = rts_v3d_add(e->pos, rts_v3d_scale(e->vel, dt_s));

        if (vp->is_submarine) {
            if (intent->jump_pressed) vp->target_depth_m = rts_clampd(vp->target_depth_m + 18.0 * dt_s, -120.0, 0.0);
            if (intent->crouch_pressed) vp->target_depth_m = rts_clampd(vp->target_depth_m - 18.0 * dt_s, -120.0, 0.0);
            vp->ballast_depth_m += (vp->target_depth_m - vp->ballast_depth_m) * rts_clampd(dt_s * 2.5, 0.0, 1.0);
        } else {
            vp->ballast_depth_m = 0.0;
        }
        RtsVec3d nu = rts_v3d_norm(e->pos);
        double wave_h = (vp->ballast_depth_m > -3.0) ? (0.45 * sin(w->sim_time_s * 1.8 + e->pos.x * 0.04)) : 0.0;
        e->pos = rts_v3d_scale(nu, w->planet.radius_m + w->planet.sea_level_m + vp->ballast_depth_m + wave_h);
        vp->wave_roll_rad = (vp->ballast_depth_m > -4.0) ? (0.04 * sin(w->sim_time_s * 1.4)) : 0.0;
        vp->wave_pitch_rad = (vp->ballast_depth_m > -4.0) ? (0.03 * cos(w->sim_time_s * 1.2)) : 0.0;
        vp->wake_intensity = (float)rts_clampd(rts_v3d_len(e->vel) / fmax(max_spd, 1.0), 0.0, 1.0);

        RtsQuatd qy = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), -yaw);
        RtsQuatd qr = rts_quat_from_axis_angle(rts_v3d(0.0, 0.0, 1.0), vp->wave_roll_rad);
        e->rot = rts_quat_norm(rts_quat_mul(qy, qr));
    } else if (e->construct_class == RTS_CLASS_AIRCRAFT) {
        /* VTOL hover <-> forward aerodynamic lift/drag flight */
        double thrust_up = (intent->jump_pressed ? 1.0 : 0.0) - (intent->crouch_pressed ? 1.0 : 0.0);
        double speed_frac = rts_clampd(rts_v3d_len(e->vel) / fmax(max_spd, 1.0), 0.0, 1.0);
        vp->vtol_blend = 1.0 - speed_frac;
        RtsVec3d desired = rts_v3d_add(rts_v3d_scale(fwd, intent->move_y * max_spd),
                                       rts_v3d_add(rts_v3d_scale(right, intent->move_x * max_spd * (0.4 + 0.4 * vp->vtol_blend)),
                                                   rts_v3d_scale(up, thrust_up * max_spd * 0.65)));
        e->vel = rts_v3d_add(rts_v3d_scale(e->vel, 0.88), rts_v3d_scale(desired, 0.12));
        e->pos = rts_v3d_add(e->pos, rts_v3d_scale(e->vel, dt_s));

        RtsVec3d nu = rts_v3d_norm(e->pos);
        double ground_h = fmax(rts_planet_local_elevation(&w->planet, nu), w->planet.sea_level_m);
        double alt = rts_planet_altitude(&w->planet, e->pos);
        if (alt < ground_h + 2.0) {
            e->pos = rts_v3d_scale(nu, w->planet.radius_m + ground_h + 2.0);
            if (rts_v3d_dot(e->vel, nu) < 0.0) e->vel = rts_v3d_sub(e->vel, rts_v3d_scale(nu, rts_v3d_dot(e->vel, nu)));
        }
        vp->airspeed_mps = rts_v3d_len(e->vel);
        vp->altitude_agl_m = fmax(0.0, rts_planet_altitude(&w->planet, e->pos) - ground_h);
        e->orbital_mode = (vp->altitude_agl_m > 80000.0);
        e->rot = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), -yaw);
    } else if (e->construct_class == RTS_CLASS_SPACECRAFT || e->construct_class == RTS_CLASS_SPACE_STATION) {
        /* 6-DOF Newtonian thruster acceleration, orbital propagation & re-entry plasma heat envelope */
        double thrust_up = (intent->jump_pressed ? 1.0 : 0.0) - (intent->crouch_pressed ? 1.0 : 0.0);
        double accel_mag = fmax(max_spd * 0.8, 30.0);
        RtsVec3d thrust = rts_v3d_add(rts_v3d_scale(fwd, intent->move_y * accel_mag),
                                      rts_v3d_add(rts_v3d_scale(right, intent->move_x * accel_mag),
                                                  rts_v3d_scale(up, thrust_up * accel_mag)));
        double alt = rts_planet_altitude(&w->planet, e->pos);
        if (e->orbital_mode || alt > 80000.0) {
            RtsOrbitState st = {e->pos, e->vel};
            st = rts_orbit_propagate_step(&w->planet, st, thrust, 1e-4, dt_s);
            e->pos = st.pos_m;
            e->vel = st.vel_mps;
            e->orbital_mode = (rts_planet_altitude(&w->planet, e->pos) > 40000.0);
        } else {
            e->vel = rts_v3d_add(rts_v3d_scale(e->vel, 0.94), rts_v3d_scale(thrust, dt_s));
            e->pos = rts_v3d_add(e->pos, rts_v3d_scale(e->vel, dt_s));
        }
        double cur_alt = rts_planet_altitude(&w->planet, e->pos);
        double spd = rts_v3d_len(e->vel);
        vp->prograde_dv_mps = spd;
        vp->reentry_heat_wm2 = rts_planet_reentry_heat_flux(&w->planet, cur_alt, spd);
        vp->reentry_glow = (float)rts_clampd(vp->reentry_heat_wm2 / 250000.0, 0.0, 1.0);
        e->rot = rts_quat_from_axis_angle(rts_v3d(0.0, 1.0, 0.0), -yaw);
    }

    if (e->own_frame_id != RTS_FRAME_INVALID) {
        RtsSpatialFrame *fr = rts_interior_get_frame(&w->interiors, e->own_frame_id);
        if (fr) { fr->local_pos = e->pos; fr->local_rot = e->rot; fr->local_vel = e->vel; }
    }

    /* Synchronize any Ground Vehicle clamped inside Dropship cargo bay */
    if (vp->clamped_cargo_id != 0u) {
        RtsEntity *cargo = rts_world_get_entity(w, vp->clamped_cargo_id);
        if (cargo) {
            cargo->pos = rts_v3d(e->pos.x, e->pos.y - 1.2, e->pos.z);
            cargo->vel = e->vel;
            cargo->rot = e->rot;
            cargo->orbital_mode = e->orbital_mode;
            if (cargo->own_frame_id != RTS_FRAME_INVALID) {
                RtsSpatialFrame *cfr = rts_interior_get_frame(&w->interiors, cargo->own_frame_id);
                if (cfr) { cfr->local_pos = cargo->pos; cfr->local_rot = cargo->rot; cfr->local_vel = cargo->vel; }
            }
        } else {
            vp->clamped_cargo_id = 0u;
        }
    }
}
