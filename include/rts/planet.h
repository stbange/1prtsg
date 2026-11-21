/* SPDX-License-Identifier: MIT */
#ifndef RTS_PLANET_H
#define RTS_PLANET_H

#include "rts/math3d.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum RtsCubeFace {
    RTS_FACE_POS_X = 0,
    RTS_FACE_NEG_X = 1,
    RTS_FACE_POS_Y = 2,
    RTS_FACE_NEG_Y = 3,
    RTS_FACE_POS_Z = 4,
    RTS_FACE_NEG_Z = 5
} RtsCubeFace;

typedef struct RtsPlanetConfig {
    char name[64];
    double radius_m;           /* Mean spherical radius (e.g. 6,371,000 m) */
    double mu;                 /* Standard gravitational parameter G*M (m^3/s^2) */
    double sea_level_m;        /* Sea level offset relative to radius_m */
    double atmo_ceiling_m;     /* Vacuum boundary altitude (e.g. 120,000 m) */
    double atmo_scale_height;  /* Exponential scale height H (e.g. 8,500 m) */
    double atmo_rho0;          /* Sea-level air density kg/m^3 (e.g. 1.225) */
    double terrain_amp_m;      /* Peak terrain relief (e.g. 4,500 m) */
    uint32_t seed;
} RtsPlanetConfig;

typedef struct RtsEnuFrame {
    RtsVec3d origin;
    RtsVec3d east;
    RtsVec3d north;
    RtsVec3d up;
    RtsQuatd orientation;
} RtsEnuFrame;

typedef struct RtsOrbitElements {
    double semi_major_axis_m;
    double eccentricity;
    double inclination_rad;
    double raan_rad;           /* Right ascension of ascending node (Omega) */
    double arg_periapsis_rad;  /* Argument of periapsis (omega) */
    double true_anomaly_rad;   /* True anomaly (nu) */
    double periapsis_alt_m;
    double apoapsis_alt_m;
    double period_s;
    bool is_bound;
} RtsOrbitElements;

typedef struct RtsOrbitState {
    RtsVec3d pos_m;
    RtsVec3d vel_mps;
} RtsOrbitState;

typedef struct RtsHohmannTransfer {
    double dv1_mps;
    double dv2_mps;
    double total_dv_mps;
    double transfer_time_s;
    bool valid;
} RtsHohmannTransfer;

RtsPlanetConfig rts_planet_earthlike(uint32_t seed);
RtsVec3d rts_planet_cube_to_sphere(RtsCubeFace face, double u, double v);
RtsVec3d rts_planet_latlon_to_unit(double lat_rad, double lon_rad);
double rts_planet_terrain_height(const RtsPlanetConfig *cfg, RtsVec3d unit_dir);
bool rts_planet_is_ocean(const RtsPlanetConfig *cfg, RtsVec3d unit_dir);
RtsVec3d rts_planet_surface_point(const RtsPlanetConfig *cfg, double lat_rad, double lon_rad, double alt_offset_m);
RtsEnuFrame rts_planet_enu_frame(const RtsPlanetConfig *cfg, double lat_rad, double lon_rad, double alt_offset_m);

double rts_planet_altitude(const RtsPlanetConfig *cfg, RtsVec3d pos_m);
RtsVec3d rts_planet_gravity_accel(const RtsPlanetConfig *cfg, RtsVec3d pos_m);
double rts_planet_atmo_density(const RtsPlanetConfig *cfg, double altitude_m);
double rts_planet_reentry_heat_flux(const RtsPlanetConfig *cfg, double altitude_m, double speed_mps);

double rts_orbit_circular_speed(const RtsPlanetConfig *cfg, double altitude_m);
RtsOrbitState rts_orbit_circular_state(const RtsPlanetConfig *cfg, double altitude_m, double inclination_rad, double phase_rad);
RtsOrbitElements rts_orbit_from_state(const RtsPlanetConfig *cfg, RtsOrbitState state);
RtsOrbitState rts_orbit_propagate_step(const RtsPlanetConfig *cfg, RtsOrbitState state, RtsVec3d thrust_accel_mps2, double drag_coeff_area_over_mass, double dt_s);
size_t rts_orbit_sample_points(const RtsPlanetConfig *cfg, RtsOrbitState state, RtsVec3d *out_pts, size_t max_pts);
RtsHohmannTransfer rts_orbit_hohmann(const RtsPlanetConfig *cfg, double r1_alt_m, double r2_alt_m);

#endif /* RTS_PLANET_H */
