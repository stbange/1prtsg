/* SPDX-License-Identifier: MIT */
#ifndef RTS_VEHICLE_PHYSICS_H
#define RTS_VEHICLE_PHYSICS_H

#include "rts/input.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct RtsVehiclePhysicsState {
    uint32_t entity_id;
    RtsConstructClass active_class;

    /* Ground Vehicles */
    double hover_clearance_m;
    double slope_pitch_rad;
    double slope_roll_rad;
    double turret_yaw_rad;
    double turret_pitch_rad;

    /* Naval Ships & Submarines */
    bool is_submarine;
    double ballast_depth_m;      /* 0.0 = surface, down to -120.0 m submerged */
    double target_depth_m;
    double wave_roll_rad;
    double wave_pitch_rad;
    float wake_intensity;

    /* Aircraft & Dropships */
    double vtol_blend;           /* 1.0 = full VTOL hover, 0.0 = high-speed wing flight */
    double airspeed_mps;
    double altitude_agl_m;
    bool ramp_open;
    uint32_t clamped_cargo_id;   /* Friendly Ground Vehicle clamped inside Dropship bay */

    /* Spacecraft & Orbital Flight */
    double prograde_dv_mps;
    double reentry_heat_wm2;
    float reentry_glow;          /* 0..1 plasma envelope intensity */
    bool docking_clamped;
    uint32_t docked_station_id;

    /* Safe Single-Player Time Compression (1x / 2x / 4x / 8x) */
    int time_compression;
    bool time_warp_blocked_combat;
} RtsVehiclePhysicsState;

void rts_vehicle_physics_init(RtsVehiclePhysicsState *vp);

/* Configure physics profile for a specific construct entity. */
void rts_vehicle_physics_bind(RtsVehiclePhysicsState *vp, const RtsWorld *w, uint32_t entity_id);

/* Integrate class-specific piloting physics (Ground, Naval/Submarine, Aircraft/Dropship, Spacecraft/6-DOF). */
void rts_vehicle_physics_step(RtsVehiclePhysicsState *vp, RtsWorld *w, const RtsInputIntent *intent, double dt_s);

/* Clamp a nearby friendly Ground Vehicle into a Dropship/Lifter bay (or release on surface). */
bool rts_vehicle_clamp_cargo(RtsVehiclePhysicsState *vp, RtsWorld *w, uint32_t carrier_id, uint32_t cargo_id);
bool rts_vehicle_release_cargo(RtsVehiclePhysicsState *vp, RtsWorld *w);

/* Safe single-player Time Compression (1, 2, 4, 8). Resets to 1 if combat_active is true. */
int rts_time_compression_set(RtsVehiclePhysicsState *vp, int factor, bool combat_active);
int rts_time_compression_cycle(RtsVehiclePhysicsState *vp, bool combat_active);

#endif /* RTS_VEHICLE_PHYSICS_H */
