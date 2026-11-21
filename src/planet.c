/* SPDX-License-Identifier: MIT */
#include "rts/planet.h"
#include <math.h>
#include <string.h>

RtsPlanetConfig rts_planet_earthlike(uint32_t seed) {
    RtsPlanetConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    strncpy(cfg.name, "Helios Prime", sizeof(cfg.name) - 1);
    cfg.radius_m = 6371000.0;
    cfg.mu = 3.986004418e14;
    cfg.sea_level_m = 0.0;
    cfg.atmo_ceiling_m = 120000.0;
    cfg.atmo_scale_height = 8500.0;
    cfg.atmo_rho0 = 1.225;
    cfg.terrain_amp_m = 4500.0;
    cfg.seed = seed;
    return cfg;
}

RtsVec3d rts_planet_cube_to_sphere(RtsCubeFace face, double u, double v) {
    double cu = rts_clampd(u, -1.0, 1.0);
    double cv = rts_clampd(v, -1.0, 1.0);
    RtsVec3d p = rts_v3d(0.0, 0.0, 0.0);
    switch (face) {
        case RTS_FACE_POS_X: p = rts_v3d( 1.0,  cv, -cu); break;
        case RTS_FACE_NEG_X: p = rts_v3d(-1.0,  cv,  cu); break;
        case RTS_FACE_POS_Y: p = rts_v3d( cu,  1.0, -cv); break;
        case RTS_FACE_NEG_Y: p = rts_v3d( cu, -1.0,  cv); break;
        case RTS_FACE_POS_Z: p = rts_v3d( cu,  cv,  1.0); break;
        case RTS_FACE_NEG_Z: p = rts_v3d(-cu,  cv, -1.0); break;
    }
    /* Spherified cube projection for uniform quadtree cell distribution */
    double x2 = p.x * p.x;
    double y2 = p.y * p.y;
    double z2 = p.z * p.z;
    RtsVec3d s = rts_v3d(
        p.x * sqrt(fmax(0.0, 1.0 - y2 * 0.5 - z2 * 0.5 + (y2 * z2) / 3.0)),
        p.y * sqrt(fmax(0.0, 1.0 - z2 * 0.5 - x2 * 0.5 + (z2 * x2) / 3.0)),
        p.z * sqrt(fmax(0.0, 1.0 - x2 * 0.5 - y2 * 0.5 + (x2 * y2) / 3.0))
    );
    return rts_v3d_norm(s);
}

RtsVec3d rts_planet_latlon_to_unit(double lat_rad, double lon_rad) {
    double clat = cos(lat_rad);
    return rts_v3d(clat * cos(lon_rad), sin(lat_rad), clat * sin(lon_rad));
}

double rts_planet_terrain_height(const RtsPlanetConfig *cfg, RtsVec3d unit_dir) {
    RtsVec3d n = rts_v3d_norm(unit_dir);
    double ph = (double)(cfg->seed % 997u) * 0.031;
    double continent = sin(n.x * 3.1 + ph) * cos(n.z * 2.7 - ph) + 0.5 * sin(n.y * 4.3 + n.x * 2.0);
    double ridge = 0.35 * sin(n.x * 11.0 - n.y * 9.0 + ph) * cos(n.z * 13.0 + ph);
    double detail = 0.12 * cos((n.x + n.y + n.z) * 29.0);
    double combined = (continent * 0.55 + ridge + detail);
    return combined * cfg->terrain_amp_m;
}

bool rts_planet_is_ocean(const RtsPlanetConfig *cfg, RtsVec3d unit_dir) {
    return rts_planet_terrain_height(cfg, unit_dir) < cfg->sea_level_m;
}

RtsVec3d rts_planet_surface_point(const RtsPlanetConfig *cfg, double lat_rad, double lon_rad, double alt_offset_m) {
    RtsVec3d u = rts_planet_latlon_to_unit(lat_rad, lon_rad);
    double h = rts_planet_terrain_height(cfg, u);
    if (h < cfg->sea_level_m) {
        h = cfg->sea_level_m;
    }
    return rts_v3d_scale(u, cfg->radius_m + h + alt_offset_m);
}

static RtsQuatd quat_from_basis(RtsVec3d right, RtsVec3d up, RtsVec3d fwd_neg_z) {
    /* Basis columns: X = right (East), Y = up (Radial), Z = -north (South/OpenGL +Z) */
    double m00 = right.x, m01 = up.x, m02 = fwd_neg_z.x;
    double m10 = right.y, m11 = up.y, m12 = fwd_neg_z.y;
    double m20 = right.z, m21 = up.z, m22 = fwd_neg_z.z;
    double trace = m00 + m11 + m22;
    RtsQuatd q;
    if (trace > 0.0) {
        double s = sqrt(trace + 1.0) * 2.0;
        q.w = 0.25 * s;
        q.x = (m21 - m12) / s;
        q.y = (m02 - m20) / s;
        q.z = (m10 - m01) / s;
    } else if (m00 > m11 && m00 > m22) {
        double s = sqrt(1.0 + m00 - m11 - m22) * 2.0;
        q.w = (m21 - m12) / s;
        q.x = 0.25 * s;
        q.y = (m01 + m10) / s;
        q.z = (m02 + m20) / s;
    } else if (m11 > m22) {
        double s = sqrt(1.0 + m11 - m00 - m22) * 2.0;
        q.w = (m02 - m20) / s;
        q.x = (m01 + m10) / s;
        q.y = 0.25 * s;
        q.z = (m12 + m21) / s;
    } else {
        double s = sqrt(1.0 + m22 - m00 - m11) * 2.0;
        q.w = (m10 - m01) / s;
        q.x = (m02 + m20) / s;
        q.y = (m12 + m21) / s;
        q.z = 0.25 * s;
    }
    return rts_quat_norm(q);
}

RtsEnuFrame rts_planet_enu_frame(const RtsPlanetConfig *cfg, double lat_rad, double lon_rad, double alt_offset_m) {
    RtsEnuFrame f;
    f.up = rts_planet_latlon_to_unit(lat_rad, lon_rad);
    RtsVec3d pole = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d east = rts_v3d_cross(pole, f.up);
    if (rts_v3d_len_sq(east) < 1e-10) {
        east = rts_v3d(1.0, 0.0, 0.0);
    } else {
        east = rts_v3d_norm(east);
    }
    RtsVec3d north = rts_v3d_norm(rts_v3d_cross(f.up, east));
    f.east = east;
    f.north = north;
    f.origin = rts_planet_surface_point(cfg, lat_rad, lon_rad, alt_offset_m);
    f.orientation = quat_from_basis(east, f.up, rts_v3d_scale(north, -1.0));
    return f;
}

double rts_planet_altitude(const RtsPlanetConfig *cfg, RtsVec3d pos_m) {
    return rts_v3d_len(pos_m) - cfg->radius_m;
}

RtsVec3d rts_planet_gravity_accel(const RtsPlanetConfig *cfg, RtsVec3d pos_m) {
    double r2 = rts_v3d_len_sq(pos_m);
    if (r2 < 1.0) {
        return rts_v3d(0.0, 0.0, 0.0);
    }
    double r = sqrt(r2);
    double mag = -cfg->mu / (r2 * r);
    return rts_v3d_scale(pos_m, mag);
}

double rts_planet_atmo_density(const RtsPlanetConfig *cfg, double altitude_m) {
    if (altitude_m >= cfg->atmo_ceiling_m) {
        return 0.0;
    }
    double h = altitude_m < 0.0 ? 0.0 : altitude_m;
    return cfg->atmo_rho0 * exp(-h / cfg->atmo_scale_height);
}

double rts_planet_reentry_heat_flux(const RtsPlanetConfig *cfg, double altitude_m, double speed_mps) {
    double rho = rts_planet_atmo_density(cfg, altitude_m);
    if (rho <= 0.0 || speed_mps <= 0.0) {
        return 0.0;
    }
    /* Simplified Sutton-Graves convective heating proxy (kW/m^2 scale) */
    return 1.83e-7 * sqrt(rho) * speed_mps * speed_mps * speed_mps;
}

double rts_orbit_circular_speed(const RtsPlanetConfig *cfg, double altitude_m) {
    double r = cfg->radius_m + altitude_m;
    if (r <= 1.0) {
        return 0.0;
    }
    return sqrt(cfg->mu / r);
}

RtsOrbitState rts_orbit_circular_state(const RtsPlanetConfig *cfg, double altitude_m, double inclination_rad, double phase_rad) {
    double r = cfg->radius_m + altitude_m;
    double v = rts_orbit_circular_speed(cfg, altitude_m);
    double cp = cos(phase_rad);
    double sp = sin(phase_rad);
    double ci = cos(inclination_rad);
    double si = sin(inclination_rad);

    RtsOrbitState st;
    st.pos_m = rts_v3d(r * cp, r * sp * si, r * sp * ci);
    st.vel_mps = rts_v3d(-v * sp, v * cp * si, v * cp * ci);
    return st;
}

RtsOrbitElements rts_orbit_from_state(const RtsPlanetConfig *cfg, RtsOrbitState state) {
    RtsOrbitElements el;
    memset(&el, 0, sizeof(el));

    double r = rts_v3d_len(state.pos_m);
    double v2 = rts_v3d_len_sq(state.vel_mps);
    if (r <= 1.0 || cfg->mu <= 0.0) {
        return el;
    }

    RtsVec3d h = rts_v3d_cross(state.pos_m, state.vel_mps);
    double h_len = rts_v3d_len(h);
    RtsVec3d k_axis = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d n_vec = rts_v3d_cross(k_axis, h);
    double n_len = rts_v3d_len(n_vec);

    RtsVec3d vxh = rts_v3d_cross(state.vel_mps, h);
    RtsVec3d e_vec = rts_v3d_sub(rts_v3d_scale(vxh, 1.0 / cfg->mu), rts_v3d_scale(state.pos_m, 1.0 / r));
    double e = rts_v3d_len(e_vec);

    double energy = 0.5 * v2 - cfg->mu / r;
    el.eccentricity = e;
    if (h_len > 1e-9) {
        el.inclination_rad = acos(rts_clampd(h.y / h_len, -1.0, 1.0));
    }
    if (n_len > 1e-9) {
        el.raan_rad = acos(rts_clampd(n_vec.x / n_len, -1.0, 1.0));
        if (n_vec.z < 0.0) {
            el.raan_rad = 2.0 * RTS_PI - el.raan_rad;
        }
    }
    if (n_len > 1e-9 && e > 1e-7) {
        el.arg_periapsis_rad = acos(rts_clampd(rts_v3d_dot(n_vec, e_vec) / (n_len * e), -1.0, 1.0));
        if (e_vec.y < 0.0) {
            el.arg_periapsis_rad = 2.0 * RTS_PI - el.arg_periapsis_rad;
        }
    }
    if (e > 1e-7) {
        el.true_anomaly_rad = acos(rts_clampd(rts_v3d_dot(e_vec, state.pos_m) / (e * r), -1.0, 1.0));
        if (rts_v3d_dot(state.pos_m, state.vel_mps) < 0.0) {
            el.true_anomaly_rad = 2.0 * RTS_PI - el.true_anomaly_rad;
        }
    } else {
        el.true_anomaly_rad = atan2(state.pos_m.z, state.pos_m.x);
    }

    if (energy < -1e-9) {
        el.is_bound = true;
        el.semi_major_axis_m = -cfg->mu / (2.0 * energy);
        el.periapsis_alt_m = el.semi_major_axis_m * (1.0 - e) - cfg->radius_m;
        el.apoapsis_alt_m = el.semi_major_axis_m * (1.0 + e) - cfg->radius_m;
        el.period_s = 2.0 * RTS_PI * sqrt((el.semi_major_axis_m * el.semi_major_axis_m * el.semi_major_axis_m) / cfg->mu);
    } else {
        el.is_bound = false;
        el.semi_major_axis_m = -cfg->mu / (2.0 * (energy == 0.0 ? 1e-9 : energy));
        el.periapsis_alt_m = (h_len * h_len / (cfg->mu * (1.0 + e))) - cfg->radius_m;
        el.apoapsis_alt_m = -1.0;
        el.period_s = 0.0;
    }
    return el;
}

static RtsVec3d total_orbit_accel(const RtsPlanetConfig *cfg, RtsVec3d pos, RtsVec3d vel, RtsVec3d thrust, double drag_factor) {
    RtsVec3d a = rts_v3d_add(rts_planet_gravity_accel(cfg, pos), thrust);
    if (drag_factor > 0.0) {
        double alt = rts_planet_altitude(cfg, pos);
        double rho = rts_planet_atmo_density(cfg, alt);
        if (rho > 0.0) {
            double speed = rts_v3d_len(vel);
            a = rts_v3d_sub(a, rts_v3d_scale(vel, 0.5 * rho * speed * drag_factor));
        }
    }
    return a;
}

RtsOrbitState rts_orbit_propagate_step(const RtsPlanetConfig *cfg, RtsOrbitState state, RtsVec3d thrust_accel_mps2, double drag_coeff_area_over_mass, double dt_s) {
    /* Velocity-Verlet symplectic step for high orbital energy stability */
    RtsVec3d a0 = total_orbit_accel(cfg, state.pos_m, state.vel_mps, thrust_accel_mps2, drag_coeff_area_over_mass);
    RtsVec3d p1 = rts_v3d_add(
        state.pos_m,
        rts_v3d_add(rts_v3d_scale(state.vel_mps, dt_s), rts_v3d_scale(a0, 0.5 * dt_s * dt_s))
    );
    RtsVec3d v_mid = rts_v3d_add(state.vel_mps, rts_v3d_scale(a0, dt_s));
    RtsVec3d a1 = total_orbit_accel(cfg, p1, v_mid, thrust_accel_mps2, drag_coeff_area_over_mass);
    RtsVec3d v1 = rts_v3d_add(state.vel_mps, rts_v3d_scale(rts_v3d_add(a0, a1), 0.5 * dt_s));

    RtsOrbitState out = {p1, v1};
    return out;
}

size_t rts_orbit_sample_points(const RtsPlanetConfig *cfg, RtsOrbitState state, RtsVec3d *out_pts, size_t max_pts) {
    if (!out_pts || max_pts < 2) {
        return 0;
    }
    RtsOrbitElements el = rts_orbit_from_state(cfg, state);
    double span_s = (el.is_bound && el.period_s > 10.0) ? el.period_s : 1800.0;
    double step_s = span_s / (double)(max_pts - 1);
    RtsOrbitState cur = state;
    out_pts[0] = cur.pos_m;
    for (size_t i = 1; i < max_pts; ++i) {
        /* Sub-step for crisp ellipse smoothness */
        int sub = 4;
        double sub_dt = step_s / (double)sub;
        for (int s = 0; s < sub; ++s) {
            cur = rts_orbit_propagate_step(cfg, cur, rts_v3d(0.0, 0.0, 0.0), 0.0, sub_dt);
        }
        out_pts[i] = cur.pos_m;
    }
    return max_pts;
}

RtsHohmannTransfer rts_orbit_hohmann(const RtsPlanetConfig *cfg, double r1_alt_m, double r2_alt_m) {
    RtsHohmannTransfer h;
    memset(&h, 0, sizeof(h));
    double r1 = cfg->radius_m + r1_alt_m;
    double r2 = cfg->radius_m + r2_alt_m;
    if (r1 <= 1.0 || r2 <= 1.0 || cfg->mu <= 0.0) {
        return h;
    }
    double a_t = 0.5 * (r1 + r2);
    double v1 = sqrt(cfg->mu / r1);
    double v2 = sqrt(cfg->mu / r2);
    double vt1 = sqrt(cfg->mu * (2.0 / r1 - 1.0 / a_t));
    double vt2 = sqrt(cfg->mu * (2.0 / r2 - 1.0 / a_t));
    h.dv1_mps = fabs(vt1 - v1);
    h.dv2_mps = fabs(v2 - vt2);
    h.total_dv_mps = h.dv1_mps + h.dv2_mps;
    h.transfer_time_s = RTS_PI * sqrt((a_t * a_t * a_t) / cfg->mu);
    h.valid = true;
    return h;
}
