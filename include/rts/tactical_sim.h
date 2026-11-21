/* SPDX-License-Identifier: MIT */
#ifndef RTS_TACTICAL_SIM_H
#define RTS_TACTICAL_SIM_H

#include "rts/audio.h"
#include "rts/math3d.h"
#include "rts/world.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define RTS_TAC_MAX_ENEMIES     32
#define RTS_TAC_MAX_PROJECTILES 96
#define RTS_TAC_MAX_PARTICLES   192
#define RTS_TAC_MAX_PICKUPS     12
#define RTS_TAC_MAX_POPUPS      8

typedef enum RtsTacticalActionId {
    RTS_ACT_PLASMA_LANCE  = 0,
    RTS_ACT_EMP_PULSE     = 1,
    RTS_ACT_THRUSTER_DASH = 2,
    RTS_ACT_FLEET_STRIKE  = 3,
    RTS_ACT_FIELD_REPAIR  = 4,
    RTS_ACT_COUNT         = 5
} RtsTacticalActionId;

typedef enum RtsEnemyArchetype {
    RTS_ENEMY_INTERCEPTOR    = 0,
    RTS_ENEMY_SHIELD_CRUISER = 1,
    RTS_ENEMY_SIEGE_WALKER   = 2
} RtsEnemyArchetype;

typedef enum RtsJuiceChannelMask {
    RTS_JUICE_ANIM_SQUASH    = 1u << 0,
    RTS_JUICE_PARTICLES      = 1u << 1,
    RTS_JUICE_CAMERA_HITSTOP = 1u << 2,
    RTS_JUICE_AUDIO          = 1u << 3,
    RTS_JUICE_UI_POP         = 1u << 4
} RtsJuiceChannelMask;

typedef enum RtsWavePhase {
    RTS_WAVE_ASSAULT  = 0,
    RTS_WAVE_BREATHER = 1
} RtsWavePhase;

typedef struct RtsFeelConfig {
    bool nonlinear_motion;
    double input_buffer_s;          /* 0.12s patched, 0.0s baseline */
    double coyote_time_s;           /* 0.10s patched, 0.0s baseline */
    bool juice_enabled;             /* hitstop, shake, particles, audio, ui pop, squash */
    double hitstop_s;               /* 0.045s patched, 0.0s baseline */
    float max_shake_vp_frac;        /* <= 0.026 (< 0.04 limit) */
    bool wave_director_enabled;     /* true patched, false flat linear spawn */
    bool strategic_synergy_enabled; /* heat + shield resist + EMP prime crit */
    double telegraph_lead_s;        /* 0.42s patched, 0.0s baseline */
    double spawn_telegraph_s;       /* 0.45s patched, 0.0s baseline */
    double min_spawn_dist_frac;     /* 0.30 patched (>= 0.25), 0.10 baseline */
    double post_hit_iframes_s;      /* 0.65s patched, 0.0s baseline */
    float player_hurt_radius_m;     /* 0.78m patched (area 0.61), 1.00m baseline (area 1.00) */
    float player_sprite_radius_m;   /* 1.00m visual exo-suit */
    float attack_hitbox_generosity; /* 1.22 patched, 1.00 baseline */
    bool instant_in_memory_retry;   /* true patched (0.27s), false cold reload (2.4s) */
    double boot_ttfa_s;             /* 0.45s patched, 3.60s baseline */
    double retry_guard_s;           /* 0.25s accidental-input guard */
    double enemy_fire_rate_scale;   /* tuning multiplier */
    double enemy_dmg_scale;         /* tuning multiplier */
} RtsFeelConfig;

typedef struct RtsTacticalInput {
    float move_x;
    float move_z;
    float aim_yaw;
    float aim_pitch;
    bool jump_pressed;
    bool action_pressed[RTS_ACT_COUNT];
    bool retry_pressed;
} RtsTacticalInput;

typedef struct RtsTacEnemy {
    uint32_t id;
    RtsEnemyArchetype archetype;
    RtsVec3f pos;
    RtsVec3f vel;
    float radius_m;
    float hull;
    float max_hull;
    float shield;
    float max_shield;
    float fire_cooldown_s;
    float telegraph_timer_s;
    float telegraph_total_s;
    RtsVec3f telegraph_aim_dir;
    float spawn_warmup_s;
    float emp_primed_timer_s;
    float squash_timer_s;
    float hit_flash_s;
    bool active;
} RtsTacEnemy;

typedef struct RtsTacProjectile {
    RtsVec3f pos;
    RtsVec3f vel;
    float radius_m;
    float damage;
    float life_s;
    bool from_player;
    bool is_emp_crit;
    bool near_miss_counted;
    bool active;
} RtsTacProjectile;

typedef struct RtsTacParticle {
    RtsVec3f pos;
    RtsVec3f vel;
    RtsVec4f color;
    float size_m;
    float life_s;
    float max_life_s;
    bool active;
} RtsTacParticle;

typedef struct RtsTacPickup {
    RtsVec3f pos;
    float radius_m;
    float energy_bonus;
    float shield_bonus;
    float life_s;
    bool active;
} RtsTacPickup;

typedef struct RtsTacPopup {
    char text[48];
    RtsVec4f color;
    float timer_s;
    float max_timer_s;
    bool active;
} RtsTacPopup;

typedef struct RtsTacTickSummary {
    bool any_action_attempted;
    RtsTacticalActionId last_action;
    bool last_action_accepted;
    bool last_action_dropped_near_ready;
    uint32_t visual_response_delay_frames;
    uint32_t juice_channels_core_sum;
    uint32_t juice_core_events;
    uint32_t juice_channels_heavy_sum;
    uint32_t juice_heavy_events;
    uint32_t audio_triggers_this_tick;
    uint32_t near_misses_this_tick;
    float reward_delta;
    bool action_executed[RTS_ACT_COUNT];
} RtsTacTickSummary;

typedef struct RtsTacticalSim {
    RtsFeelConfig cfg;
    uint64_t seed;
    uint64_t rng;

    /* Arena & lifecycle */
    float arena_half_m;
    double sim_time_s;
    double max_duration_s;
    uint32_t tick_index;
    bool active_run;
    bool victory;
    bool game_over;
    double game_over_time_s;
    double boot_ms;
    double first_interactive_ms;
    double run_start_ms;
    double game_over_ms;
    double restart_ready_ms;

    /* Player kinematic & combat state */
    RtsVec3f player_pos;
    RtsVec3f player_vel;
    RtsVec3f player_accel;
    float player_yaw;
    float player_pitch;
    bool player_grounded;
    float coyote_timer_s;
    float dash_timer_s;
    RtsVec3f dash_dir;
    float player_squash;
    float player_squash_timer_s;

    float health;
    float max_health;
    float exo_shield;
    float max_exo_shield;
    float shield_regen_delay_s;
    float iframe_timer_s;
    float tactical_energy;
    float max_tactical_energy;
    float weapon_heat;
    float overheat_timer_s;

    float cooldown_s[RTS_ACT_COUNT];
    float input_buffer_ttl_s[RTS_ACT_COUNT];
    uint32_t input_buffer_press_tick[RTS_ACT_COUNT];
    uint32_t input_buffer_ready_tick[RTS_ACT_COUNT];
    bool jump_buffered;
    float jump_buffer_ttl_s;

    /* Juice state */
    float hitstop_timer_s;
    uint32_t hitstop_ticks_total;
    float camera_trauma;
    RtsVec3f camera_shake_offset;
    float reticle_flash_s;
    float damage_vignette_s;
    float last_audio_pitch;

    /* Wave Director & scoring */
    int wave_number;
    RtsWavePhase wave_phase;
    double wave_phase_timer_s;
    double spawn_timer_s;
    uint32_t next_enemy_id;
    uint32_t enemies_killed;
    uint32_t waves_cleared;
    uint32_t near_misses_total;
    uint32_t emp_crits_total;
    uint32_t combo_count;
    float combo_timer_s;
    float score;
    double last_micro_reward_s;
    double last_meso_reward_s;

    /* Pools */
    RtsTacEnemy enemies[RTS_TAC_MAX_ENEMIES];
    RtsTacProjectile projectiles[RTS_TAC_MAX_PROJECTILES];
    RtsTacParticle particles[RTS_TAC_MAX_PARTICLES];
    RtsTacPickup pickups[RTS_TAC_MAX_PICKUPS];
    RtsTacPopup popups[RTS_TAC_MAX_POPUPS];

    /* Optional linked audio engine for live spatial SFX triggers */
    RtsAudioEngine *audio;
    RtsTacTickSummary last_tick;
} RtsTacticalSim;

RtsFeelConfig rts_feel_config_baseline(void);
RtsFeelConfig rts_feel_config_solaris(void);

void rts_tactical_init(RtsTacticalSim *sim, const RtsFeelConfig *cfg, uint64_t seed, RtsAudioEngine *audio);
bool rts_tactical_reset_fast(RtsTacticalSim *sim, uint64_t new_seed);
void rts_tactical_step(RtsTacticalSim *sim, const RtsTacticalInput *in, double dt_s);
void rts_tactical_sync_to_world(const RtsTacticalSim *sim, RtsWorld *world);

uint32_t rts_tactical_active_enemy_count(const RtsTacticalSim *sim);
uint32_t rts_tactical_active_particle_count(const RtsTacticalSim *sim);
float rts_tactical_nearest_threat_pw(const RtsTacticalSim *sim);
float rts_tactical_compute_tension(const RtsTacticalSim *sim, float recent_input_rate_hz);
float rts_tactical_completion_score(const RtsTacticalSim *sim);

#endif /* RTS_TACTICAL_SIM_H */
