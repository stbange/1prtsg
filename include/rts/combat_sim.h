/* SPDX-License-Identifier: MIT */
#ifndef RTS_COMBAT_SIM_H
#define RTS_COMBAT_SIM_H

#include "rts/interior_gen.h"
#include "rts/voxel.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_COMBAT_MAX_PROJ 128
#define RTS_COMBAT_MAX_RIPPLES 32
#define RTS_COMBAT_MAX_CRATERS 16

typedef enum RtsWeaponKind {
    RTS_WPN_PLASMA_BOLT = 0,
    RTS_WPN_BEAM_LANCE = 1,
    RTS_WPN_HOMING_TORPEDO = 2,
    RTS_WPN_AA_FLAK = 3,
    RTS_WPN_ORBITAL_ION = 4,
    RTS_WPN_EMP_PULSE = 5
} RtsWeaponKind;

typedef struct RtsLiveProjectile {
    RtsWeaponKind kind;
    uint32_t shooter_id;
    uint32_t target_id;
    uint8_t faction_id;
    RtsVec3d pos;
    RtsVec3d origin;
    RtsVec3d vel;
    float damage;
    float speed_mps;
    float age_s;
    float max_life_s;
    bool active;
} RtsLiveProjectile;

typedef struct RtsShieldRipple {
    uint32_t entity_id;
    RtsVec3f local_dir;
    float timer_s;
    float duration_s;
    float intensity;
    bool emp_tint;
    bool active;
} RtsShieldRipple;

typedef struct RtsVoxelCraterSite {
    RtsVec3d world_pos;
    float radius_m;
    bool active;
} RtsVoxelCraterSite;

typedef struct RtsCombatSim {
    RtsLiveProjectile projs[RTS_COMBAT_MAX_PROJ];
    RtsShieldRipple ripples[RTS_COMBAT_MAX_RIPPLES];
    RtsVoxelCraterSite craters[RTS_COMBAT_MAX_CRATERS];
    size_t crater_count;

    float emp_prime_timer[RTS_MAX_ENTITIES];
    float weapon_cooldown[RTS_MAX_ENTITIES];

    RtsVoxelChunk destructible_patch;
    bool voxel_patch_initialized;
    uint32_t total_shots_fired;
    uint32_t total_hits;
    uint32_t emp_combo_hits;
    uint32_t subsystem_breaches;
} RtsCombatSim;

void rts_combat_init(RtsCombatSim *cs);

/* Select signature weapon family for a construct class & orbital state. */
RtsWeaponKind rts_combat_weapon_for_entity(const RtsEntity *e);

/* Determine which physical compartment hitbox was struck from local impact offset. */
RtsSubsystemKind rts_combat_hit_subsystem(const RtsEntity *target, RtsVec3d impact_world);

/* Fire a live 64-bit projectile or beam lance between entities (or toward a world target). */
bool rts_combat_fire(RtsCombatSim *cs, const RtsWorld *w, uint32_t shooter_id,
                     uint32_t target_id, RtsVec3d target_pos, RtsWeaponKind kind);

/* Apply EMP-Prime status (targets take x2.4 synergy damage while primed). */
void rts_combat_apply_emp_prime(RtsCombatSim *cs, const RtsWorld *w, uint32_t entity_id, float duration_s);
bool rts_combat_is_emp_primed(const RtsCombatSim *cs, const RtsWorld *w, uint32_t entity_id);

/* Carve a destructible voxel crater on the local voxel terrain/asteroid patch. */
int rts_combat_carve_voxel_crater(RtsCombatSim *cs, RtsVec3d world_pos, float radius_m);

/* Advance live-world combat (AI firing, homing, EMP x2.4 synergy, shield ripples, compartment damage -> mega interior). */
void rts_combat_step(RtsCombatSim *cs, RtsWorld *w, RtsMegaInterior *active_mega, double dt_s);

/* Returns true if any projectiles are flying or hostile combatants are within danger range (disables time warp). */
bool rts_combat_is_active(const RtsCombatSim *cs, const RtsWorld *w);

#endif /* RTS_COMBAT_SIM_H */
