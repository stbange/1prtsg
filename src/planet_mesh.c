/* SPDX-License-Identifier: MIT */
#include "rts/planet_mesh.h"
#include <math.h>
#include <string.h>

static void tangent_basis(RtsVec3d up, RtsVec3d *out_east, RtsVec3d *out_north) {
    RtsVec3d ref = (fabs(up.y) < 0.92) ? rts_v3d(0.0, 1.0, 0.0) : rts_v3d(0.0, 0.0, 1.0);
    RtsVec3d east = rts_v3d_norm(rts_v3d_cross(ref, up));
    RtsVec3d north = rts_v3d_norm(rts_v3d_cross(up, east));
    *out_east = east;
    *out_north = north;
}

RtsPlanetLodSpec rts_planet_select_lod(double altitude_m) {
    RtsPlanetLodSpec s;
    memset(&s, 0, sizeof(s));
    if (altitude_m < 600.0) {
        s.lod_level = 0;
        s.half_span_m = 460.0;
        s.grid_res = RTS_PLANET_PATCH_GRID;
        s.render_ocean = true;
        s.render_clouds = false;
        s.atmo_blend = 0.0f;
    } else if (altitude_m < 8000.0) {
        s.lod_level = 1;
        s.half_span_m = 3200.0;
        s.grid_res = RTS_PLANET_PATCH_GRID;
        s.render_ocean = true;
        s.render_clouds = true;
        s.atmo_blend = 0.22f;
    } else if (altitude_m < 60000.0) {
        s.lod_level = 2;
        s.half_span_m = 38000.0;
        s.grid_res = 16;
        s.render_ocean = true;
        s.render_clouds = true;
        s.atmo_blend = 0.58f;
    } else if (altitude_m < 500000.0) {
        s.lod_level = 3;
        s.half_span_m = 280000.0;
        s.grid_res = 14;
        s.render_ocean = true;
        s.render_clouds = true;
        s.atmo_blend = 0.85f;
    } else {
        s.lod_level = 4;
        s.half_span_m = 1200000.0;
        s.grid_res = 12;
        s.render_ocean = false;
        s.render_clouds = true;
        s.atmo_blend = 1.0f;
    }
    return s;
}

double rts_planet_local_elevation(const RtsPlanetConfig *cfg, RtsVec3d unit_dir) {
    if (!cfg) return 0.0;
    RtsVec3d u = rts_v3d_norm(unit_dir);
    /* Global cube-sphere relief everywhere on Helios Prime */
    double global_h = rts_planet_terrain_height(cfg, u);

    /* Near the Solaris Citadel landing theater (0, R, 0), sculpt a scenic dry plateau + accessible harbor bay */
    double dx_m = u.x * cfg->radius_m;
    double dz_m = u.z * cfg->radius_m;
    double dist_theater_m = sqrt(dx_m * dx_m + dz_m * dz_m);
    if (u.y > 0.99 && dist_theater_m < 2400.0) {
        double t = rts_clampd((dist_theater_m - 120.0) / 1400.0, 0.0, 1.0);
        /* Harbor bay opens cleanly to the east/southeast (+X / +Z > 115m) for Naval/Submarine ops */
        double bay_slope = (dx_m > 90.0) ? (-0.28 * (dx_m - 90.0)) : 0.0;
        double ridge_nw = (dx_m < -65.0 || dz_m < -85.0)
                        ? (14.0 + 18.0 * sin(dx_m * 0.015) * cos(dz_m * 0.015) + 0.07 * fabs(dx_m + 65.0))
                        : 0.0;
        double gentle_mound = 0.9 * sin(dx_m * 0.025) * cos(dz_m * 0.025);
        double local_h = (dist_theater_m < 88.0) ? (gentle_mound * 0.25) : (bay_slope + ridge_nw + gentle_mound);
        local_h = rts_clampd(local_h, -110.0, 140.0);
        return local_h * (1.0 - t) + global_h * 0.08 * t;
    }
    return global_h;
}

bool rts_planet_local_is_ocean(const RtsPlanetConfig *cfg, RtsVec3d unit_dir) {
    if (!cfg) return false;
    return rts_planet_local_elevation(cfg, unit_dir) < cfg->sea_level_m;
}

RtsVec3d rts_planet_local_normal(const RtsPlanetConfig *cfg, RtsVec3d unit_dir, double sample_step_m) {
    if (!cfg) return rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d u = rts_v3d_norm(unit_dir);
    RtsVec3d east, north;
    tangent_basis(u, &east, &north);
    double step_rad = fmax(sample_step_m, 1.0) / fmax(cfg->radius_m, 1000.0);

    RtsVec3d ue = rts_v3d_norm(rts_v3d_add(u, rts_v3d_scale(east, step_rad)));
    RtsVec3d un = rts_v3d_norm(rts_v3d_add(u, rts_v3d_scale(north, step_rad)));
    double h0 = rts_planet_local_elevation(cfg, u);
    double he = rts_planet_local_elevation(cfg, ue);
    double hn = rts_planet_local_elevation(cfg, un);

    RtsVec3d p0 = rts_v3d_scale(u, cfg->radius_m + h0);
    RtsVec3d pe = rts_v3d_scale(ue, cfg->radius_m + he);
    RtsVec3d pn = rts_v3d_scale(un, cfg->radius_m + hn);
    RtsVec3d nrm = rts_v3d_cross(rts_v3d_sub(pn, p0), rts_v3d_sub(pe, p0));
    if (rts_v3d_dot(nrm, u) < 0.0) nrm = rts_v3d_scale(nrm, -1.0);
    return rts_v3d_norm(nrm);
}

static void terrain_biome_color(double elev_m, double sea_m, float *r, float *g, float *b) {
    double rel = elev_m - sea_m;
    if (rel < -12.0) {
        *r = 0.08f; *g = 0.22f; *b = 0.36f; /* Deep seabed */
    } else if (rel < 1.5) {
        *r = 0.72f; *g = 0.78f; *b = 0.76f; /* Ivory-titanium shoreline */
    } else if (rel < 22.0) {
        *r = 0.36f; *g = 0.56f; *b = 0.62f; /* Solaris coastal steppe */
    } else if (rel < 85.0) {
        *r = 0.44f; *g = 0.54f; *b = 0.66f; /* Glacier slate highlands */
    } else {
        *r = 0.86f; *g = 0.92f; *b = 0.98f; /* Alpine snowpeaks */
    }
}

static RtsVertex make_vert(RtsVec3d world_p, RtsVec3d cam_world, RtsVec3d nrm, float u, float v, float r, float g, float b) {
    RtsVertex vt;
    RtsVec3f rel = rts_v3d_relative_f(world_p, cam_world);
    vt.px = rel.x; vt.py = rel.y; vt.pz = rel.z;
    vt.nx = (float)nrm.x; vt.ny = (float)nrm.y; vt.nz = (float)nrm.z;
    vt.u = u; vt.v = v;
    vt.r = r; vt.g = g; vt.b = b;
    return vt;
}

size_t rts_planet_build_surface_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                      RtsPlanetLodSpec lod, RtsVertex *out_verts, size_t max_verts) {
    if (!cfg || !out_verts || max_verts < 6u) return 0u;
    int res = lod.grid_res;
    if (res < 2) res = 2;
    if (res > RTS_PLANET_PATCH_GRID) res = RTS_PLANET_PATCH_GRID;

    RtsVec3d up = rts_v3d_norm(cam_world);
    if (rts_v3d_len_sq(up) < 1e-9) up = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d east, north;
    tangent_basis(up, &east, &north);

    double span = lod.half_span_m;
    double step_m = (2.0 * span) / (double)res;
    size_t count = 0u;

    for (int gz = 0; gz < res; ++gz) {
        double z0 = -span + (double)gz * step_m;
        double z1 = z0 + step_m;
        for (int gx = 0; gx < res; ++gx) {
            if (count + 6u > max_verts) return count;
            double x0 = -span + (double)gx * step_m;
            double x1 = x0 + step_m;

            double cx[4] = {x0, x1, x1, x0};
            double cz[4] = {z0, z0, z1, z1};
            RtsVertex q[4];

            for (int k = 0; k < 4; ++k) {
                RtsVec3d tangent_p = rts_v3d_add(rts_v3d_scale(up, cfg->radius_m),
                                                 rts_v3d_add(rts_v3d_scale(east, cx[k]), rts_v3d_scale(north, cz[k])));
                RtsVec3d udir = rts_v3d_norm(tangent_p);
                double elev = rts_planet_local_elevation(cfg, udir);
                RtsVec3d wp = rts_v3d_scale(udir, cfg->radius_m + elev);
                RtsVec3d nrm = rts_planet_local_normal(cfg, udir, step_m * 0.5);
                float r, g, b;
                terrain_biome_color(elev, cfg->sea_level_m, &r, &g, &b);
                float uv_u = (float)((cx[k] + span) / (2.0 * span));
                float uv_v = (float)((cz[k] + span) / (2.0 * span));
                q[k] = make_vert(wp, cam_world, nrm, uv_u, uv_v, r, g, b);
            }
            out_verts[count++] = q[0];
            out_verts[count++] = q[2];
            out_verts[count++] = q[1];
            out_verts[count++] = q[0];
            out_verts[count++] = q[3];
            out_verts[count++] = q[2];
        }
    }
    return count;
}

size_t rts_planet_build_ocean_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                    RtsPlanetLodSpec lod, float time_s,
                                    RtsVertex *out_verts, size_t max_verts) {
    if (!cfg || !out_verts || max_verts < 6u || !lod.render_ocean) return 0u;
    int res = lod.grid_res > 14 ? 14 : lod.grid_res;
    if (res < 2) res = 2;

    RtsVec3d up = rts_v3d_norm(cam_world);
    if (rts_v3d_len_sq(up) < 1e-9) up = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d east, north;
    tangent_basis(up, &east, &north);

    double span = lod.half_span_m;
    double step_m = (2.0 * span) / (double)res;
    size_t count = 0u;

    for (int gz = 0; gz < res; ++gz) {
        double z0 = -span + (double)gz * step_m;
        double z1 = z0 + step_m;
        for (int gx = 0; gx < res; ++gx) {
            if (count + 6u > max_verts) return count;
            double x0 = -span + (double)gx * step_m;
            double x1 = x0 + step_m;
            double cx[4] = {x0, x1, x1, x0};
            double cz[4] = {z0, z0, z1, z1};
            RtsVertex q[4];
            bool any_water = false;

            for (int k = 0; k < 4; ++k) {
                RtsVec3d tp = rts_v3d_add(rts_v3d_scale(up, cfg->radius_m),
                                          rts_v3d_add(rts_v3d_scale(east, cx[k]), rts_v3d_scale(north, cz[k])));
                RtsVec3d udir = rts_v3d_norm(tp);
                double bed = rts_planet_local_elevation(cfg, udir);
                if (bed < cfg->sea_level_m + 2.0) any_water = true;
                double wave = 0.55 * sin(cx[k] * 0.06 + (double)time_s * 1.9) * cos(cz[k] * 0.05 - (double)time_s * 1.3);
                double depth = cfg->sea_level_m - bed;
                float foam = (depth > -1.0 && depth < 5.0) ? 0.35f : 0.0f;
                RtsVec3d wp = rts_v3d_scale(udir, cfg->radius_m + cfg->sea_level_m + wave);
                float r = 0.12f + foam;
                float g = 0.56f + foam * 0.7f + (float)(0.05 * sin(cx[k] * 0.03 + (double)time_s));
                float b = 0.84f + foam * 0.3f;
                q[k] = make_vert(wp, cam_world, udir, (float)gx / (float)res, (float)gz / (float)res, r, g, b);
            }
            if (!any_water) continue;
            out_verts[count++] = q[0];
            out_verts[count++] = q[2];
            out_verts[count++] = q[1];
            out_verts[count++] = q[0];
            out_verts[count++] = q[3];
            out_verts[count++] = q[2];
        }
    }
    return count;
}

size_t rts_planet_build_cloud_patch(const RtsPlanetConfig *cfg, RtsVec3d cam_world,
                                    float time_s, RtsVertex *out_verts, size_t max_verts) {
    if (!cfg || !out_verts || max_verts < 6u) return 0u;
    int res = 8;
    RtsVec3d up = rts_v3d_norm(cam_world);
    if (rts_v3d_len_sq(up) < 1e-9) up = rts_v3d(0.0, 1.0, 0.0);
    RtsVec3d east, north;
    tangent_basis(up, &east, &north);

    double span = 120000.0;
    double step = (2.0 * span) / (double)res;
    double drift = (double)time_s * 45.0;
    size_t count = 0u;

    for (int gz = 0; gz < res; ++gz) {
        double z0 = -span + (double)gz * step;
        double z1 = z0 + step;
        for (int gx = 0; gx < res; ++gx) {
            if (count + 6u > max_verts) return count;
            double x0 = -span + (double)gx * step;
            double x1 = x0 + step;
            double cx[4] = {x0, x1, x1, x0};
            double cz[4] = {z0, z0, z1, z1};
            RtsVertex q[4];
            for (int k = 0; k < 4; ++k) {
                RtsVec3d tp = rts_v3d_add(rts_v3d_scale(up, cfg->radius_m),
                                          rts_v3d_add(rts_v3d_scale(east, cx[k] + drift), rts_v3d_scale(north, cz[k])));
                RtsVec3d udir = rts_v3d_norm(tp);
                RtsVec3d wp = rts_v3d_scale(udir, cfg->radius_m + 18000.0);
                q[k] = make_vert(wp, cam_world, udir, (float)gx / (float)res, (float)gz / (float)res, 0.88f, 0.94f, 1.0f);
            }
            out_verts[count++] = q[0];
            out_verts[count++] = q[2];
            out_verts[count++] = q[1];
            out_verts[count++] = q[0];
            out_verts[count++] = q[3];
            out_verts[count++] = q[2];
        }
    }
    return count;
}
