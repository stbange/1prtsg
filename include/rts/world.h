/* SPDX-License-Identifier: MIT */
#ifndef RTS_WORLD_H
#define RTS_WORLD_H

#include "rts/interior.h"
#include "rts/math3d.h"
#include "rts/planet.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_MAX_ENTITIES 256
#define RTS_MAX_BLUEPRINTS 128
#define RTS_MAX_CREW_ROLES 24
#define RTS_MAX_PROJECTILES 128

typedef enum RtsConstructClass {
    RTS_CLASS_BUILDING = 0,
    RTS_CLASS_GROUND_VEHICLE = 1,
    RTS_CLASS_NAVAL_SHIP = 2,
    RTS_CLASS_AIRCRAFT = 3,
    RTS_CLASS_SPACECRAFT = 4,
    RTS_CLASS_SPACE_STATION = 5,
    RTS_CLASS_CREW = 6
} RtsConstructClass;

typedef enum RtsSubsystemKind {
    RTS_SUBSYS_REACTOR = 0,
    RTS_SUBSYS_ENGINES = 1,
    RTS_SUBSYS_SENSORS = 2,
    RTS_SUBSYS_WEAPONS = 3,
    RTS_SUBSYS_BRIDGE = 4,
    RTS_SUBSYS_COUNT = 5
} RtsSubsystemKind;

typedef enum RtsOrderType {
    RTS_ORDER_NONE = 0,
    RTS_ORDER_MOVE = 1,
    RTS_ORDER_ATTACK = 2,
    RTS_ORDER_ORBIT = 3,
    RTS_ORDER_DOCK = 4,
    RTS_ORDER_BUILD = 5,
    RTS_ORDER_REPAIR = 6,
    RTS_ORDER_EVACUATE = 7
} RtsOrderType;

typedef enum RtsCrewLodMode {
    RTS_CREW_LOD_3D_MESH = 0,
    RTS_CREW_LOD_SPRITE = 1
} RtsCrewLodMode;

typedef enum RtsMatchState {
    RTS_MATCH_ACTIVE = 0,
    RTS_MATCH_VICTORY = 1,
    RTS_MATCH_DEFEAT_IRONMAN = 2
} RtsMatchState;

typedef struct RtsConstructBlueprint {
    char id[64];
    char name[96];
    RtsConstructClass construct_class;
    RtsVec3f dimensions_m;
    float max_hull;
    float max_shield;
    float max_speed_mps;
    uint32_t crew_capacity;
    uint32_t room_count;
    bool has_holo_globe;
    bool has_escape_pod;
    bool can_orbit;
    char mesh_path[128];
    bool active;
} RtsConstructBlueprint;

typedef struct RtsCrewRoleDef {
    char id[48];
    char name[64];
    char function_desc[128];
    bool active;
} RtsCrewRoleDef;

typedef struct RtsEntity {
    uint32_t id;
    char blueprint_id[64];
    char name[96];
    RtsConstructClass construct_class;
    uint8_t faction_id;
    RtsFrameId host_frame_id;
    RtsFrameId own_frame_id;
    RtsVec3d pos;
    RtsVec3d vel;
    RtsQuatd rot;
    RtsVec3f dimensions_m;

    float hull;
    float max_hull;
    float shield;
    float max_shield;
    float shield_regen_per_s;
    float subsystem_hp[RTS_SUBSYS_COUNT];
    float max_speed_mps;

    uint32_t crew_count;
    uint32_t engineer_count;
    uint32_t assigned_role_idx;

    RtsOrderType order_type;
    RtsVec3d order_target_pos;
    uint32_t order_target_entity;
    double order_target_altitude_m;

    bool orbital_mode;
    bool has_holo_globe;
    bool has_escape_pod;
    bool shield_hit_flash;
    float shield_hit_timer;
    bool active;
} RtsEntity;

typedef struct RtsCommanderState {
    RtsFrameId frame_id;
    RtsVec3d local_pos;
    RtsVec3d local_vel;
    double yaw_rad;
    double pitch_rad;
    float health;
    float max_health;
    float exo_shield;
    float max_exo_shield;
    bool mag_boots;
    bool wrist_pad_open;
    bool at_holo_globe;
    bool in_escape_pod;
    bool alive;
    uint32_t occupied_entity_id;
} RtsCommanderState;

typedef struct RtsWorld {
    RtsPlanetConfig planet;
    RtsInteriorSystem interiors;
    RtsFrameId planet_frame_id;

    RtsConstructBlueprint blueprints[RTS_MAX_BLUEPRINTS];
    size_t blueprint_count;
    RtsCrewRoleDef crew_roles[RTS_MAX_CREW_ROLES];
    size_t crew_role_count;

    RtsEntity entities[RTS_MAX_ENTITIES];
    size_t entity_count;
    uint32_t next_entity_id;

    RtsCommanderState commander;
    RtsMatchState match_state;
    bool hardcore_ironman;
    double sim_time_s;
    uint64_t tick_count;
    uint64_t rng_state;

    double resources_ore;
    double resources_fuel;
    double resources_power;
} RtsWorld;

void rts_world_init(RtsWorld *w, RtsPlanetConfig planet, uint64_t rng_seed);
double rts_world_rand01(RtsWorld *w);

bool rts_world_register_blueprint(RtsWorld *w, const RtsConstructBlueprint *bp);
const RtsConstructBlueprint *rts_world_find_blueprint(const RtsWorld *w, const char *id);
bool rts_world_register_crew_role(RtsWorld *w, const RtsCrewRoleDef *role);

uint32_t rts_world_spawn_from_blueprint(RtsWorld *w, const char *blueprint_id, const char *custom_name, uint8_t faction_id, RtsFrameId host_frame, RtsVec3d pos, bool create_interior_frame);
RtsEntity *rts_world_get_entity(RtsWorld *w, uint32_t id);
const RtsEntity *rts_world_get_entity_const(const RtsWorld *w, uint32_t id);
bool rts_world_destroy_entity(RtsWorld *w, uint32_t id);

bool rts_world_issue_order(RtsWorld *w, uint32_t entity_id, RtsOrderType order, RtsVec3d target_pos, uint32_t target_entity, double target_alt_m);
void rts_world_apply_damage(RtsWorld *w, uint32_t entity_id, float damage, RtsSubsystemKind subsys);
float rts_world_effective_speed(const RtsEntity *e);

RtsCrewLodMode rts_world_eval_crew_lod(double dist_m, bool inside_interior, bool webxr_active, double sprite_threshold_m, double view_azimuth_rad, int dir_count, int *out_dir_index);

bool rts_world_commander_launch_escape_pod(RtsWorld *w);
void rts_world_damage_commander(RtsWorld *w, float damage);
void rts_world_step(RtsWorld *w, double dt_s);

#endif /* RTS_WORLD_H */
