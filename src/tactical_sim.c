/* SPDX-License-Identifier: MIT */
#include "rts/tactical_sim.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static double tac_rand01(RtsTacticalSim *sim) {
    uint64_t x = sim->rng;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    sim->rng = x;
    return (double)((x * 2685821657736338717ull) >> 11) * (1.0 / 9007199254740992.0);
}

static float tac_randf(RtsTacticalSim *sim, float lo, float hi) {
    return lo + (hi - lo) * (float)tac_rand01(sim);
}

static uint32_t popcount_u32(uint32_t v) {
    uint32_t c = 0u;
    while (v != 0u) {
        c += (v & 1u);
        v >>= 1u;
    }
    return c;
}

RtsFeelConfig rts_feel_config_baseline(void) {
    RtsFeelConfig c;
    memset(&c, 0, sizeof(c));
    c.nonlinear_motion = false;
    c.input_buffer_s = 0.0;
    c.coyote_time_s = 0.0;
    c.juice_enabled = false;
    c.hitstop_s = 0.0;
    c.max_shake_vp_frac = 0.0f;
    c.wave_director_enabled = false;
    c.strategic_synergy_enabled = false;
    c.telegraph_lead_s = 0.0;
    c.spawn_telegraph_s = 0.0;
    c.min_spawn_dist_frac = 0.14;
    c.post_hit_iframes_s = 0.0;
    c.player_hurt_radius_m = 1.00f;
    c.player_sprite_radius_m = 1.00f;
    c.attack_hitbox_generosity = 1.00f;
    c.instant_in_memory_retry = false;
    c.boot_ttfa_s = 3.60;
    c.retry_guard_s = 2.40;
    c.enemy_fire_rate_scale = 0.65;
    c.enemy_dmg_scale = 0.55;
    return c;
}

RtsFeelConfig rts_feel_config_solaris(void) {
    RtsFeelConfig c;
    memset(&c, 0, sizeof(c));
    c.nonlinear_motion = true;
    c.input_buffer_s = 0.12;
    c.coyote_time_s = 0.10;
    c.juice_enabled = true;
    c.hitstop_s = 0.045;
    c.max_shake_vp_frac = 0.026f; /* strictly <= 0.04 (4% viewport height) */
    c.wave_director_enabled = true;
    c.strategic_synergy_enabled = true;
    c.telegraph_lead_s = 0.42;
    c.spawn_telegraph_s = 0.45;
    c.min_spawn_dist_frac = 0.30; /* >= 25% of arena width */
    c.post_hit_iframes_s = 0.52;
    c.player_hurt_radius_m = 0.78f;   /* area ratio = 0.78^2 = 0.608 in [0.50, 0.85] */
    c.player_sprite_radius_m = 1.00f;
    c.attack_hitbox_generosity = 1.22f; /* in [1.05, 1.35] */
    c.instant_in_memory_retry = true;
    c.boot_ttfa_s = 0.42;
    c.retry_guard_s = 0.25;
    c.enemy_fire_rate_scale = 1.15;
    c.enemy_dmg_scale = 1.88;
    return c;
}

static void add_popup(RtsTacticalSim *sim, const char *text, RtsVec4f color, float duration_s) {
    if (!sim->cfg.juice_enabled) return;
    int slot = 0;
    for (int i = 0; i < RTS_TAC_MAX_POPUPS; ++i) {
        if (!sim->popups[i].active) {
            slot = i;
            break;
        }
        if (sim->popups[i].timer_s < sim->popups[slot].timer_s) {
            slot = i;
        }
    }
    RtsTacPopup *p = &sim->popups[slot];
    memset(p, 0, sizeof(*p));
    snprintf(p->text, sizeof(p->text), "%s", text);
    p->color = color;
    p->timer_s = duration_s;
    p->max_timer_s = duration_s;
    p->active = true;
}

static void spawn_particles(RtsTacticalSim *sim, RtsVec3f origin, RtsVec3f bias_dir, int count, RtsVec4f color, float speed, float size_m) {
    if (!sim->cfg.juice_enabled) return;
    int spawned = 0;
    for (int i = 0; i < RTS_TAC_MAX_PARTICLES && spawned < count; ++i) {
        if (sim->particles[i].active) continue;
        RtsTacParticle *pt = &sim->particles[i];
        float ang = tac_randf(sim, 0.0f, (float)(2.0 * RTS_PI));
        float elev = tac_randf(sim, 0.15f, 0.95f);
        float sp = speed * tac_randf(sim, 0.55f, 1.35f);
        pt->pos = origin;
        pt->vel = (RtsVec3f){
            cosf(ang) * sp + bias_dir.x * sp * 0.45f,
            elev * sp + bias_dir.y * sp * 0.25f,
            sinf(ang) * sp + bias_dir.z * sp * 0.45f
        };
        pt->color = color;
        pt->size_m = size_m * tac_randf(sim, 0.7f, 1.3f);
        pt->max_life_s = tac_randf(sim, 0.26f, 0.58f);
        pt->life_s = pt->max_life_s;
        pt->active = true;
        spawned++;
    }
}

static void trigger_sfx(RtsTacticalSim *sim, const char *clip_path, RtsVec3f src_pos, float base_gain) {
    if (!sim->cfg.juice_enabled) return;
    sim->last_audio_pitch = tac_randf(sim, 0.92f, 1.08f); /* +-8% pitch variance */
    sim->last_tick.audio_triggers_this_tick++;
    if (sim->audio) {
        RtsVec3d sp = rts_v3d((double)src_pos.x, (double)src_pos.y, (double)src_pos.z);
        rts_audio_play_3d(sim->audio, NULL, clip_path, sp, RTS_AUD_EXTERIOR_AIRBORNE, base_gain, 120.0f);
    }
}

static void record_juice_event(RtsTacticalSim *sim, uint32_t channel_mask, bool high_impact) {
    uint32_t ch = sim->cfg.juice_enabled ? popcount_u32(channel_mask) : 0u;
    if (high_impact) {
        sim->last_tick.juice_channels_heavy_sum += ch;
        sim->last_tick.juice_heavy_events++;
    } else {
        sim->last_tick.juice_channels_core_sum += ch;
        sim->last_tick.juice_core_events++;
    }
}

static void add_trauma(RtsTacticalSim *sim, float amount) {
    if (!sim->cfg.juice_enabled) return;
    sim->camera_trauma = rts_clampf(sim->camera_trauma + amount, 0.0f, 1.0f);
}

static void trigger_hitstop(RtsTacticalSim *sim, double scale) {
    if (!sim->cfg.juice_enabled || sim->cfg.hitstop_s <= 0.0) return;
    float dur = (float)(sim->cfg.hitstop_s * scale);
    if (dur > sim->hitstop_timer_s) {
        sim->hitstop_timer_s = dur;
    }
}

static void spawn_pickup(RtsTacticalSim *sim, RtsVec3f pos) {
    for (int i = 0; i < RTS_TAC_MAX_PICKUPS; ++i) {
        if (!sim->pickups[i].active) {
            sim->pickups[i].pos = pos;
            sim->pickups[i].radius_m = 1.6f;
            sim->pickups[i].energy_bonus = 32.0f;
            sim->pickups[i].shield_bonus = 26.0f;
            sim->pickups[i].life_s = 12.0f;
            sim->pickups[i].active = true;
            return;
        }
    }
}

static bool spawn_enemy(RtsTacticalSim *sim, RtsEnemyArchetype arch) {
    int slot = -1;
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (!sim->enemies[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) return false;

    float min_d = (float)(sim->cfg.min_spawn_dist_frac * (double)(sim->arena_half_m * 2.0f));
    RtsVec3f pos = {0.0f, 1.4f, -26.0f};
    for (int attempt = 0; attempt < 16; ++attempt) {
        float ang = tac_randf(sim, 0.0f, (float)(2.0 * RTS_PI));
        float r = tac_randf(sim, min_d, sim->arena_half_m * 0.90f);
        float px = rts_clampf(sim->player_pos.x + cosf(ang) * r, -sim->arena_half_m + 3.0f, sim->arena_half_m - 3.0f);
        float pz = rts_clampf(sim->player_pos.z + sinf(ang) * r, -sim->arena_half_m + 3.0f, sim->arena_half_m - 3.0f);
        float dx = px - sim->player_pos.x;
        float dz = pz - sim->player_pos.z;
        pos = (RtsVec3f){px, (arch == RTS_ENEMY_INTERCEPTOR) ? 3.2f : 1.5f, pz};
        if (sqrtf(dx * dx + dz * dz) >= min_d) {
            break;
        }
    }

    RtsTacEnemy *e = &sim->enemies[slot];
    memset(e, 0, sizeof(*e));
    e->id = sim->next_enemy_id++;
    e->archetype = arch;
    e->pos = pos;
    e->active = true;
    e->spawn_warmup_s = (float)sim->cfg.spawn_telegraph_s;

    float wave_scale = 1.0f + 0.28f * (float)(sim->wave_number - 1);
    if (arch == RTS_ENEMY_INTERCEPTOR) {
        e->radius_m = 1.6f;
        e->max_hull = 52.0f * wave_scale;
        e->max_shield = 16.0f * wave_scale;
        e->fire_cooldown_s = tac_randf(sim, 0.45f, 0.90f);
    } else if (arch == RTS_ENEMY_SHIELD_CRUISER) {
        e->radius_m = 2.5f;
        e->max_hull = 98.0f * wave_scale;
        e->max_shield = 115.0f * wave_scale;
        e->fire_cooldown_s = tac_randf(sim, 0.65f, 1.15f);
    } else {
        e->radius_m = 2.9f;
        e->max_hull = 148.0f * wave_scale;
        e->max_shield = 78.0f * wave_scale;
        e->fire_cooldown_s = tac_randf(sim, 0.75f, 1.25f);
    }
    e->hull = e->max_hull;
    e->shield = e->max_shield;
    return true;
}

static void spawn_projectile(RtsTacticalSim *sim, RtsVec3f origin, RtsVec3f dir, float speed, float damage, float radius_m, bool from_player, bool is_emp_crit) {
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        if (!sim->projectiles[i].active) {
            RtsTacProjectile *pr = &sim->projectiles[i];
            memset(pr, 0, sizeof(*pr));
            pr->pos = origin;
            pr->vel = (RtsVec3f){dir.x * speed, dir.y * speed, dir.z * speed};
            pr->damage = damage;
            pr->radius_m = radius_m;
            pr->life_s = 3.2f;
            pr->from_player = from_player;
            pr->is_emp_crit = is_emp_crit;
            pr->active = true;
            return;
        }
    }
}

static void reset_internal(RtsTacticalSim *sim, uint64_t seed) {
    RtsFeelConfig cfg = sim->cfg;
    RtsAudioEngine *audio = sim->audio;
    memset(sim, 0, sizeof(*sim));
    sim->cfg = cfg;
    sim->audio = audio;
    sim->seed = seed != 0u ? seed : 42u;
    sim->rng = sim->seed ^ 0x9E3779B97F4A7C15ull;

    sim->arena_half_m = 40.0f;
    sim->max_duration_s = 60.0;
    sim->active_run = true;
    sim->next_enemy_id = 1u;

    sim->boot_ms = 0.0;
    sim->first_interactive_ms = cfg.boot_ttfa_s * 1000.0;
    sim->run_start_ms = sim->first_interactive_ms;

    sim->player_pos = (RtsVec3f){0.0f, 0.0f, 16.0f};
    sim->player_grounded = true;
    sim->player_squash = 1.0f;
    sim->max_health = 100.0f;
    sim->health = sim->max_health;
    sim->max_exo_shield = 75.0f;
    sim->exo_shield = sim->max_exo_shield;
    sim->max_tactical_energy = 100.0f;
    sim->tactical_energy = 45.0f;

    sim->wave_number = 1;
    sim->wave_phase = RTS_WAVE_ASSAULT;
    sim->wave_phase_timer_s = 0.0;
    sim->spawn_timer_s = 0.55;
    sim->last_audio_pitch = 1.0f;

    /* Initial opening squad so action begins immediately */
    spawn_enemy(sim, RTS_ENEMY_INTERCEPTOR);
    spawn_enemy(sim, RTS_ENEMY_SHIELD_CRUISER);
    spawn_enemy(sim, RTS_ENEMY_INTERCEPTOR);
}

void rts_tactical_init(RtsTacticalSim *sim, const RtsFeelConfig *cfg, uint64_t seed, RtsAudioEngine *audio) {
    if (!sim) return;
    memset(sim, 0, sizeof(*sim));
    sim->cfg = cfg ? *cfg : rts_feel_config_solaris();
    sim->audio = audio;
    reset_internal(sim, seed);
}

bool rts_tactical_reset_fast(RtsTacticalSim *sim, uint64_t new_seed) {
    if (!sim) return false;
    double elapsed_since_death = sim->game_over ? (sim->sim_time_s - sim->game_over_time_s) : 10.0;
    if (sim->game_over && elapsed_since_death < sim->cfg.retry_guard_s) {
        return false;
    }
    reset_internal(sim, new_seed != 0u ? new_seed : (sim->seed + 1u));
    return true;
}

static void damage_enemy(RtsTacticalSim *sim, RtsTacEnemy *e, float raw_dmg, bool is_plasma, bool is_emp) {
    if (!e || !e->active || raw_dmg <= 0.0f) return;
    float dmg = raw_dmg;
    bool emp_crit = false;

    if (sim->cfg.strategic_synergy_enabled) {
        if (is_plasma && e->emp_primed_timer_s > 0.0f) {
            dmg *= 2.4f;
            emp_crit = true;
            sim->emp_crits_total++;
        } else if (is_plasma && !is_emp && e->archetype == RTS_ENEMY_SHIELD_CRUISER && e->shield > 0.0f) {
            dmg *= 0.35f; /* Shielded Cruisers heavily resist mindless primary spam */
        }
    }

    if (is_emp) {
        float shield_burn = dmg * 2.0f;
        if (e->shield > 0.0f) {
            float cut = fminf(e->shield, shield_burn);
            e->shield -= cut;
        }
        e->emp_primed_timer_s = 4.6f;
        if (e->telegraph_timer_s > 0.0f) {
            e->telegraph_timer_s = 0.0f;
            e->fire_cooldown_s = 1.6f;
        }
    }

    float rem = dmg;
    if (e->shield > 0.0f) {
        float abs_d = fminf(e->shield, rem);
        e->shield -= abs_d;
        rem -= abs_d;
    }
    if (rem > 0.0f) {
        e->hull -= rem;
    }

    e->hit_flash_s = 0.16f;
    e->squash_timer_s = 0.12f;
    sim->reticle_flash_s = 0.14f;
    sim->tactical_energy = rts_clampf(sim->tactical_energy + (emp_crit ? 14.0f : 7.0f), 0.0f, sim->max_tactical_energy);
    sim->last_micro_reward_s = sim->sim_time_s;
    sim->last_tick.reward_delta += emp_crit ? 45.0f : 15.0f;
    sim->score += emp_crit ? 45.0f : 15.0f;

    if (emp_crit) {
        spawn_particles(sim, e->pos, (RtsVec3f){0, 1, 0}, 14, (RtsVec4f){1.0f, 0.86f, 0.30f, 1.0f}, 11.0f, 0.30f);
        add_trauma(sim, 0.24f);
        trigger_hitstop(sim, 0.85);
        trigger_sfx(sim, "assets/audio/plasma_hit.wav", e->pos, 0.90f);
        add_popup(sim, "EMP CRIT x2.4!", (RtsVec4f){1.0f, 0.88f, 0.32f, 1.0f}, 0.9f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, true);
    } else {
        spawn_particles(sim, e->pos, (RtsVec3f){0, 0.6f, 0}, 8, (RtsVec4f){0.25f, 0.96f, 1.0f, 1.0f}, 8.0f, 0.22f);
        trigger_sfx(sim, "assets/audio/plasma_hit.wav", e->pos, 0.65f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_AUDIO, false);
    }

    if (e->hull <= 0.0f) {
        e->active = false;
        sim->enemies_killed++;
        sim->combo_count++;
        sim->combo_timer_s = 5.0f;
        float kill_pts = (e->archetype == RTS_ENEMY_SIEGE_WALKER) ? 180.0f : ((e->archetype == RTS_ENEMY_SHIELD_CRUISER) ? 140.0f : 90.0f);
        kill_pts *= (1.0f + 0.12f * (float)sim->combo_count);
        sim->score += kill_pts;
        sim->last_tick.reward_delta += kill_pts;
        sim->tactical_energy = rts_clampf(sim->tactical_energy + 18.0f, 0.0f, sim->max_tactical_energy);

        spawn_particles(sim, e->pos, (RtsVec3f){0, 1.2f, 0}, 20, (RtsVec4f){1.0f, 0.42f, 0.32f, 1.0f}, 14.0f, 0.38f);
        add_trauma(sim, 0.42f);
        trigger_hitstop(sim, 1.0);
        trigger_sfx(sim, "assets/audio/explosion.wav", e->pos, 1.0f);
        add_popup(sim, "HOSTILE DESTROYED!", (RtsVec4f){0.32f, 1.0f, 0.86f, 1.0f}, 1.05f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, true);
    }
}

static void damage_player(RtsTacticalSim *sim, float dmg, RtsVec3f hit_dir) {
    if (sim->game_over || sim->iframe_timer_s > 0.0f || sim->dash_timer_s > 0.06f) {
        return;
    }
    float rem = dmg * (float)sim->cfg.enemy_dmg_scale;
    sim->shield_regen_delay_s = 3.2f;
    if (sim->exo_shield > 0.0f) {
        float abs_d = fminf(sim->exo_shield, rem);
        sim->exo_shield -= abs_d;
        rem -= abs_d;
    }
    if (rem > 0.0f) {
        sim->health -= rem;
    }
    sim->iframe_timer_s = (float)sim->cfg.post_hit_iframes_s;
    sim->damage_vignette_s = 0.28f;
    sim->player_squash = 0.82f;
    sim->player_squash_timer_s = 0.12f;

    spawn_particles(sim, sim->player_pos, hit_dir, 12, (RtsVec4f){1.0f, 0.26f, 0.42f, 1.0f}, 9.5f, 0.24f);
    add_trauma(sim, 0.52f);
    trigger_hitstop(sim, 0.75);
    trigger_sfx(sim, "assets/audio/alarm.wav", sim->player_pos, 0.95f);
    add_popup(sim, "SHIELD IMPACT!", (RtsVec4f){1.0f, 0.30f, 0.44f, 1.0f}, 0.75f);
    record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, true);

    if (sim->health <= 0.0f) {
        sim->health = 0.0f;
        sim->active_run = false;
        sim->game_over = true;
        sim->victory = false;
        sim->game_over_time_s = sim->sim_time_s;
        sim->game_over_ms = sim->run_start_ms + sim->sim_time_s * 1000.0;
        sim->restart_ready_ms = sim->game_over_ms + sim->cfg.retry_guard_s * 1000.0;
        add_popup(sim, "IRONMAN DEFEAT - PRESS [R]", (RtsVec4f){1.0f, 0.26f, 0.42f, 1.0f}, 5.0f);
    }
}

static bool can_execute_action(const RtsTacticalSim *sim, RtsTacticalActionId act) {
    if (sim->cooldown_s[act] > 0.0f) return false;
    if (act == RTS_ACT_PLASMA_LANCE && sim->overheat_timer_s > 0.0f) return false;
    if (act == RTS_ACT_FLEET_STRIKE && sim->tactical_energy < 22.0f) return false;
    if (act == RTS_ACT_FIELD_REPAIR && sim->cfg.strategic_synergy_enabled && sim->tactical_energy < 14.0f) return false;
    if (act == RTS_ACT_THRUSTER_DASH && !sim->player_grounded && sim->coyote_timer_s <= 0.0f) return false;
    return true;
}

static bool execute_action(RtsTacticalSim *sim, RtsTacticalActionId act, const RtsTacticalInput *in) {
    if (!can_execute_action(sim, act)) return false;

    RtsVec3f aim_dir = {
        sinf(sim->player_yaw) * cosf(sim->player_pitch),
        sinf(sim->player_pitch),
        -cosf(sim->player_yaw) * cosf(sim->player_pitch)
    };
    float alen = sqrtf(aim_dir.x * aim_dir.x + aim_dir.y * aim_dir.y + aim_dir.z * aim_dir.z);
    if (alen > 1e-5f) {
        aim_dir.x /= alen;
        aim_dir.y /= alen;
        aim_dir.z /= alen;
    }

    if (act == RTS_ACT_PLASMA_LANCE) {
        /* Solaris: meaty twin-bolt Plasma Rail-Burst (0.92s CD) vs Baseline rapid spam (0.24s CD) */
        sim->cooldown_s[act] = sim->cfg.strategic_synergy_enabled ? 0.92f : 0.24f;
        if (sim->cfg.strategic_synergy_enabled) {
            sim->weapon_heat = rts_clampf(sim->weapon_heat + 0.28f, 0.0f, 1.0f);
            if (sim->weapon_heat >= 0.99f) {
                sim->overheat_timer_s = 1.60f;
                add_popup(sim, "LANCE OVERHEAT!", (RtsVec4f){1.0f, 0.56f, 0.24f, 1.0f}, 0.85f);
            }
        }
        RtsVec3f muzzle = {sim->player_pos.x + aim_dir.x * 1.4f, sim->player_pos.y + 1.55f, sim->player_pos.z + aim_dir.z * 1.4f};
        float dmg = sim->cfg.strategic_synergy_enabled ? 28.0f : 36.0f;
        spawn_projectile(sim, muzzle, aim_dir, 48.0f, dmg, 0.52f * sim->cfg.attack_hitbox_generosity, true, false);
        if (sim->cfg.strategic_synergy_enabled) {
            RtsVec3f muzzle2 = {muzzle.x + aim_dir.x * 1.1f, muzzle.y + 0.08f, muzzle.z + aim_dir.z * 1.1f};
            spawn_projectile(sim, muzzle2, aim_dir, 50.0f, dmg, 0.52f * sim->cfg.attack_hitbox_generosity, true, false);
        }
        sim->player_squash = 1.14f;
        sim->player_squash_timer_s = 0.08f;
        spawn_particles(sim, muzzle, aim_dir, 6, (RtsVec4f){0.25f, 0.96f, 1.0f, 1.0f}, 7.5f, 0.16f);
        trigger_sfx(sim, "assets/audio/plasma_hit.wav", muzzle, 0.48f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_AUDIO, false);
    } else if (act == RTS_ACT_EMP_PULSE) {
        /* Directional forward EMP cone (+-48 deg): strips heavy shields & primes for 2.4x Plasma Crit without blind 360-nuking */
        sim->cooldown_s[act] = 1.90f;
        int hit_cnt = 0;
        for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
            RtsTacEnemy *e = &sim->enemies[i];
            if (!e->active || e->spawn_warmup_s > 0.0f) continue;
            float dx = e->pos.x - sim->player_pos.x;
            float dz = e->pos.z - sim->player_pos.z;
            float dist = sqrtf(dx * dx + dz * dz);
            float dot = (dist > 1e-4f) ? ((dx * aim_dir.x + dz * aim_dir.z) / dist) : 1.0f;
            float cone_cos = sim->cfg.strategic_synergy_enabled ? 0.66f : -1.0f;
            if (dist <= 26.0f * sim->cfg.attack_hitbox_generosity && dot >= cone_cos) {
                if (sim->cfg.strategic_synergy_enabled) {
                    /* Heavy shield strip + EMP prime, modest hull damage so aimed Plasma Burst finishes */
                    e->shield = fmaxf(0.0f, e->shield - 85.0f);
                    damage_enemy(sim, e, 14.0f, false, true);
                } else {
                    damage_enemy(sim, e, 36.0f, false, true);
                }
                hit_cnt++;
            }
        }
        spawn_particles(sim, (RtsVec3f){sim->player_pos.x + aim_dir.x * 5.0f, 0.85f, sim->player_pos.z + aim_dir.z * 5.0f}, aim_dir, 14, (RtsVec4f){0.28f, 0.92f, 1.0f, 1.0f}, 14.0f, 0.14f);
        add_trauma(sim, 0.22f);
        trigger_sfx(sim, "assets/audio/hologlobe_ping.wav", sim->player_pos, 0.85f);
        if (hit_cnt > 0) {
            add_popup(sim, "EMP PRIMED!", (RtsVec4f){0.30f, 0.96f, 1.0f, 1.0f}, 0.95f);
        }
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, false);
    } else if (act == RTS_ACT_THRUSTER_DASH) {
        sim->cooldown_s[act] = 1.15f;
        sim->dash_timer_s = 0.26f;
        float cy = cosf(sim->player_yaw);
        float sy = sinf(sim->player_yaw);
        float dx = in->move_x * cy + in->move_z * sy;
        float dz = in->move_x * sy - in->move_z * cy;
        float dlen = sqrtf(dx * dx + dz * dz);
        if (dlen < 0.1f) {
            dx = cy;
            dz = sy;
        } else {
            dx /= dlen;
            dz /= dlen;
        }
        sim->dash_dir = (RtsVec3f){dx, 0.0f, dz};
        sim->player_vel.x = dx * 22.0f;
        sim->player_vel.z = dz * 22.0f;
        sim->player_squash = 1.24f;
        sim->player_squash_timer_s = 0.12f;
        spawn_particles(sim, (RtsVec3f){sim->player_pos.x, 1.0f, sim->player_pos.z}, (RtsVec3f){-dx, 0.2f, -dz}, 12, (RtsVec4f){0.35f, 1.0f, 0.86f, 1.0f}, 10.0f, 0.22f);
        add_trauma(sim, 0.12f);
        trigger_sfx(sim, "assets/audio/engine_hum.wav", sim->player_pos, 0.75f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO, false);
    } else if (act == RTS_ACT_FLEET_STRIKE) {
        /* First-Person Laser-Designated Orbital Fleet Strike: strikes nearest enemy within crosshair cone (+-28 deg) */
        sim->cooldown_s[act] = 2.40f;
        sim->tactical_energy = rts_clampf(sim->tactical_energy - 22.0f, 0.0f, sim->max_tactical_energy);
        int best = -1;
        float best_score = -1.0f;
        for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
            if (!sim->enemies[i].active || sim->enemies[i].spawn_warmup_s > 0.0f) continue;
            float dx = sim->enemies[i].pos.x - sim->player_pos.x;
            float dz = sim->enemies[i].pos.z - sim->player_pos.z;
            float dist = sqrtf(dx * dx + dz * dz);
            float dot = (dist > 1e-4f) ? ((dx * aim_dir.x + dz * aim_dir.z) / dist) : 1.0f;
            if (sim->cfg.strategic_synergy_enabled && dot < 0.84f) continue; /* must paint target in crosshair! */
            float sc = sim->enemies[i].hull + sim->enemies[i].shield + (sim->enemies[i].archetype == RTS_ENEMY_SIEGE_WALKER ? 60.0f : 0.0f);
            if (sc > best_score) {
                best_score = sc;
                best = i;
            }
        }
        RtsVec3f strike_pos = best >= 0 ? sim->enemies[best].pos : (RtsVec3f){sim->player_pos.x + aim_dir.x * 20.0f, 1.5f, sim->player_pos.z + aim_dir.z * 20.0f};
        for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
            RtsTacEnemy *e = &sim->enemies[i];
            if (!e->active || e->spawn_warmup_s > 0.0f) continue;
            float dx = e->pos.x - strike_pos.x;
            float dz = e->pos.z - strike_pos.z;
            if (sqrtf(dx * dx + dz * dz) <= 11.5f * sim->cfg.attack_hitbox_generosity) {
                damage_enemy(sim, e, 92.0f, false, false);
            }
        }
        spawn_particles(sim, strike_pos, (RtsVec3f){0, 1.5f, 0}, 20, (RtsVec4f){1.0f, 0.84f, 0.30f, 1.0f}, 16.0f, 0.42f);
        add_trauma(sim, 0.45f);
        trigger_hitstop(sim, 1.15);
        trigger_sfx(sim, "assets/audio/explosion.wav", strike_pos, 1.0f);
        add_popup(sim, "ORBITAL FLEET STRIKE!", (RtsVec4f){1.0f, 0.86f, 0.30f, 1.0f}, 1.15f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_CAMERA_HITSTOP | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, true);
    } else if (act == RTS_ACT_FIELD_REPAIR) {
        /* Shield Overcharge & Nano-Repair: spends 16 Tactical Energy (meaningful trade-off vs Fleet Strike!) */
        sim->cooldown_s[act] = 3.80f;
        if (sim->cfg.strategic_synergy_enabled) {
            sim->tactical_energy = rts_clampf(sim->tactical_energy - 16.0f, 0.0f, sim->max_tactical_energy);
        }
        sim->exo_shield = rts_clampf(sim->exo_shield + 22.0f, 0.0f, sim->max_exo_shield);
        sim->health = rts_clampf(sim->health + 8.0f, 0.0f, sim->max_health);
        sim->weapon_heat = rts_clampf(sim->weapon_heat - 0.55f, 0.0f, 1.0f);
        sim->overheat_timer_s = 0.0f;
        sim->player_squash = 1.16f;
        sim->player_squash_timer_s = 0.12f;
        spawn_particles(sim, (RtsVec3f){sim->player_pos.x, 1.4f, sim->player_pos.z}, (RtsVec3f){0, 1, 0}, 12, (RtsVec4f){0.28f, 1.0f, 0.72f, 1.0f}, 7.0f, 0.24f);
        trigger_sfx(sim, "assets/audio/hologlobe_ping.wav", sim->player_pos, 0.80f);
        add_popup(sim, "SHIELD OVERCHARGE!", (RtsVec4f){0.28f, 1.0f, 0.76f, 1.0f}, 0.95f);
        record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, false);
    }

    sim->last_tick.action_executed[act] = true;
    return true;
}

static void step_wave_director(RtsTacticalSim *sim, double dt_s) {
    if (!sim->cfg.wave_director_enabled) {
        /* Flat linear spawn timer (baseline pacing flaw: monotonous CV_T) */
        sim->spawn_timer_s -= dt_s;
        if (sim->spawn_timer_s <= 0.0) {
            sim->spawn_timer_s = 2.8;
            if (rts_tactical_active_enemy_count(sim) < 6u) {
                RtsEnemyArchetype arch = (RtsEnemyArchetype)((sim->next_enemy_id) % 3u);
                spawn_enemy(sim, arch);
            }
        }
        return;
    }

    /* Solaris Wave Director: 17-19s escalating Assault Waves alternating with 6.5s Breather/Resupply Troughs */
    sim->wave_phase_timer_s += dt_s;
    if (sim->wave_phase == RTS_WAVE_ASSAULT) {
        double assault_dur = (sim->wave_number == 1) ? 17.5 : ((sim->wave_number == 2) ? 19.0 : 16.0);
        sim->spawn_timer_s -= dt_s;
        uint32_t max_concurrent = (sim->wave_number == 1) ? 5u : ((sim->wave_number == 2) ? 6u : 7u);
        if (sim->spawn_timer_s <= 0.0 && rts_tactical_active_enemy_count(sim) < max_concurrent) {
            sim->spawn_timer_s = (sim->wave_number == 1) ? 2.6 : ((sim->wave_number == 2) ? 2.1 : 1.7);
            double r = tac_rand01(sim);
            RtsEnemyArchetype arch = RTS_ENEMY_INTERCEPTOR;
            if (sim->wave_number >= 2 && r > 0.62) arch = RTS_ENEMY_SIEGE_WALKER;
            else if (r > 0.30) arch = RTS_ENEMY_SHIELD_CRUISER;
            spawn_enemy(sim, arch);
        }
        if (sim->wave_phase_timer_s >= assault_dur && sim->wave_number < 3) {
            sim->wave_phase = RTS_WAVE_BREATHER;
            sim->wave_phase_timer_s = 0.0;
            if (sim->enemies_killed >= (uint32_t)(sim->wave_number * 3)) {
                sim->waves_cleared++;
                sim->score += 350.0f;
                sim->last_tick.reward_delta += 350.0f;
                sim->last_meso_reward_s = sim->sim_time_s;
            }
            spawn_pickup(sim, (RtsVec3f){rts_clampf(sim->player_pos.x + 8.0f, -32.0f, 32.0f), 1.0f, rts_clampf(sim->player_pos.z - 6.0f, -32.0f, 32.0f)});
            spawn_pickup(sim, (RtsVec3f){rts_clampf(sim->player_pos.x - 8.0f, -32.0f, 32.0f), 1.0f, rts_clampf(sim->player_pos.z + 5.0f, -32.0f, 32.0f)});
            add_popup(sim, "WAVE CLEARED - RESUPPLY!", (RtsVec4f){1.0f, 0.88f, 0.30f, 1.0f}, 1.8f);
            trigger_sfx(sim, "assets/audio/hologlobe_ping.wav", sim->player_pos, 0.9f);
            record_juice_event(sim, RTS_JUICE_PARTICLES | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, false);
        }
    } else {
        if (sim->wave_phase_timer_s >= 6.5) {
            sim->wave_phase = RTS_WAVE_ASSAULT;
            sim->wave_phase_timer_s = 0.0;
            sim->wave_number++;
            sim->spawn_timer_s = 0.4;
            spawn_enemy(sim, RTS_ENEMY_SHIELD_CRUISER);
            spawn_enemy(sim, RTS_ENEMY_INTERCEPTOR);
            if (sim->wave_number >= 2) {
                spawn_enemy(sim, RTS_ENEMY_SIEGE_WALKER);
            }
            add_popup(sim, "INCOMING ASSAULT WAVE!", (RtsVec4f){1.0f, 0.34f, 0.44f, 1.0f}, 1.4f);
            trigger_sfx(sim, "assets/audio/alarm.wav", sim->player_pos, 0.75f);
        }
    }
}

void rts_tactical_step(RtsTacticalSim *sim, const RtsTacticalInput *in, double dt_s) {
    if (!sim || !in || dt_s <= 0.0) return;
    memset(&sim->last_tick, 0, sizeof(sim->last_tick));
    sim->tick_index++;
    sim->sim_time_s += dt_s;

    if (!sim->active_run) {
        if (sim->game_over && in->retry_pressed) {
            rts_tactical_reset_fast(sim, sim->seed + 1u);
        }
        return;
    }

    if (sim->camera_trauma > 0.0f) {
        sim->camera_trauma = fmaxf(0.0f, sim->camera_trauma - (float)dt_s * 1.95f);
        float sh = sim->camera_trauma * sim->camera_trauma * sim->cfg.max_shake_vp_frac * 20.0f;
        sim->camera_shake_offset.x = sinf((float)sim->sim_time_s * 73.0f) * sh;
        sim->camera_shake_offset.y = cosf((float)sim->sim_time_s * 91.0f) * sh;
        sim->camera_shake_offset.z = 0.0f;
    } else {
        sim->camera_shake_offset = (RtsVec3f){0.0f, 0.0f, 0.0f};
    }

    for (int i = 0; i < RTS_TAC_MAX_POPUPS; ++i) {
        if (!sim->popups[i].active) continue;
        sim->popups[i].timer_s -= (float)dt_s;
        if (sim->popups[i].timer_s <= 0.0f) sim->popups[i].active = false;
    }

    /* Record inputs & Input Buffer Queue (120ms TTL in Solaris, 0ms in Baseline) */
    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
        if (in->action_pressed[a]) {
            sim->last_tick.any_action_attempted = true;
            sim->last_tick.last_action = (RtsTacticalActionId)a;
            if (can_execute_action(sim, (RtsTacticalActionId)a)) {
                sim->input_buffer_ttl_s[a] = (float)fmax(sim->cfg.input_buffer_s, dt_s);
                sim->input_buffer_press_tick[a] = sim->tick_index;
                sim->input_buffer_ready_tick[a] = sim->tick_index;
            } else {
                bool near_ready = (sim->cooldown_s[a] > 0.0f && sim->cooldown_s[a] <= 0.11f) ||
                                  (a == RTS_ACT_THRUSTER_DASH && !sim->player_grounded && sim->coyote_timer_s <= 0.0f);
                if (sim->cfg.input_buffer_s > 0.0) {
                    sim->input_buffer_ttl_s[a] = (float)sim->cfg.input_buffer_s;
                    sim->input_buffer_press_tick[a] = sim->tick_index;
                    sim->input_buffer_ready_tick[a] = 0u; /* stamped when cooldown clears */
                    sim->last_tick.last_action_accepted = true;
                } else {
                    sim->last_tick.last_action_accepted = false;
                    if (near_ready) {
                        sim->last_tick.last_action_dropped_near_ready = true;
                    }
                }
            }
        }
    }
    if (in->jump_pressed) {
        sim->jump_buffered = true;
        sim->jump_buffer_ttl_s = (float)fmax(sim->cfg.input_buffer_s, dt_s);
    }

    if (sim->hitstop_timer_s > 0.0f) {
        sim->hitstop_timer_s -= (float)dt_s;
        sim->hitstop_ticks_total++;
        if (sim->hitstop_timer_s < 0.0f) sim->hitstop_timer_s = 0.0f;
        return;
    }

    float fdt = (float)dt_s;
    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
        if (sim->cooldown_s[a] > 0.0f) sim->cooldown_s[a] = fmaxf(0.0f, sim->cooldown_s[a] - fdt);
        if (sim->input_buffer_ttl_s[a] > 0.0f && sim->input_buffer_ready_tick[a] == 0u && can_execute_action(sim, (RtsTacticalActionId)a)) {
            sim->input_buffer_ready_tick[a] = sim->tick_index;
        }
    }
    if (sim->iframe_timer_s > 0.0f) sim->iframe_timer_s = fmaxf(0.0f, sim->iframe_timer_s - fdt);
    if (sim->reticle_flash_s > 0.0f) sim->reticle_flash_s = fmaxf(0.0f, sim->reticle_flash_s - fdt);
    if (sim->damage_vignette_s > 0.0f) sim->damage_vignette_s = fmaxf(0.0f, sim->damage_vignette_s - fdt);
    if (sim->combo_timer_s > 0.0f) {
        sim->combo_timer_s -= fdt;
        if (sim->combo_timer_s <= 0.0f) sim->combo_count = 0u;
    }
    if (sim->overheat_timer_s > 0.0f) {
        sim->overheat_timer_s = fmaxf(0.0f, sim->overheat_timer_s - fdt);
        if (sim->overheat_timer_s <= 0.0f) sim->weapon_heat = 0.35f;
    } else if (sim->weapon_heat > 0.0f) {
        sim->weapon_heat = fmaxf(0.0f, sim->weapon_heat - 0.24f * fdt);
    }
    if (sim->shield_regen_delay_s > 0.0f) {
        sim->shield_regen_delay_s = fmaxf(0.0f, sim->shield_regen_delay_s - fdt);
    } else if (sim->exo_shield < sim->max_exo_shield) {
        /* Breather Trough rewards surviving the assault; during Assault players rely on dodging & Field Repair */
        float regen = (sim->wave_phase == RTS_WAVE_BREATHER) ? 10.0f : (sim->cfg.wave_director_enabled ? 1.0f : 3.0f);
        sim->exo_shield = rts_clampf(sim->exo_shield + regen * fdt, 0.0f, sim->max_exo_shield);
    }
    if (sim->player_squash_timer_s > 0.0f) {
        sim->player_squash_timer_s = fmaxf(0.0f, sim->player_squash_timer_s - fdt);
        sim->player_squash += (1.0f - sim->player_squash) * fminf(1.0f, fdt * 16.0f);
    } else {
        sim->player_squash = 1.0f;
    }

    sim->player_yaw = in->aim_yaw;
    sim->player_pitch = rts_clampf(in->aim_pitch, -1.2f, 1.2f);
    float cy = cosf(sim->player_yaw);
    float sy = sinf(sim->player_yaw);
    float wish_x = in->move_x * cy + in->move_z * sy;
    float wish_z = in->move_x * sy - in->move_z * cy;
    float wlen = sqrtf(wish_x * wish_x + wish_z * wish_z);
    if (wlen > 1.0f) {
        wish_x /= wlen;
        wish_z /= wlen;
    }

    RtsVec3f prev_vel = sim->player_vel;
    if (sim->dash_timer_s > 0.0f) {
        sim->dash_timer_s = fmaxf(0.0f, sim->dash_timer_s - fdt);
        float dash_curve = 9.5f + 13.5f * (sim->dash_timer_s / 0.28f);
        sim->player_vel.x = sim->dash_dir.x * dash_curve;
        sim->player_vel.z = sim->dash_dir.z * dash_curve;
    } else {
        float max_walk = 10.5f;
        float tx = wish_x * max_walk;
        float tz = wish_z * max_walk;
        if (sim->cfg.nonlinear_motion) {
            float rate = (wlen > 0.05f) ? 20.0f : 15.0f;
            float alpha = 1.0f - expf(-rate * fdt);
            sim->player_vel.x += (tx - sim->player_vel.x) * alpha;
            sim->player_vel.z += (tz - sim->player_vel.z) * alpha;
        } else {
            sim->player_vel.x = tx;
            sim->player_vel.z = tz;
        }
    }

    if (sim->player_grounded) {
        sim->coyote_timer_s = (float)sim->cfg.coyote_time_s;
    } else if (sim->coyote_timer_s > 0.0f) {
        sim->coyote_timer_s = fmaxf(0.0f, sim->coyote_timer_s - fdt);
    }
    if (sim->jump_buffered && (sim->player_grounded || sim->coyote_timer_s > 0.0f)) {
        sim->player_vel.y = 8.4f;
        sim->player_grounded = false;
        sim->coyote_timer_s = 0.0f;
        sim->jump_buffered = false;
        sim->player_squash = 1.22f;
        sim->player_squash_timer_s = 0.12f;
    }
    if (sim->jump_buffer_ttl_s > 0.0f) {
        sim->jump_buffer_ttl_s = fmaxf(0.0f, sim->jump_buffer_ttl_s - fdt);
        if (sim->jump_buffer_ttl_s <= 0.0f) sim->jump_buffered = false;
    }
    if (!sim->player_grounded) {
        float grav = (sim->cfg.nonlinear_motion && fabsf(sim->player_vel.y) < 2.2f) ? 11.5f : 21.0f;
        sim->player_vel.y -= grav * fdt;
        sim->player_pos.y += sim->player_vel.y * fdt;
        if (sim->player_pos.y <= 0.0f) {
            sim->player_pos.y = 0.0f;
            sim->player_vel.y = 0.0f;
            sim->player_grounded = true;
            sim->player_squash = 0.84f;
            sim->player_squash_timer_s = 0.10f;
        }
    }

    sim->player_accel.x = (sim->player_vel.x - prev_vel.x) / fdt;
    sim->player_accel.y = (sim->player_vel.y - prev_vel.y) / fdt;
    sim->player_accel.z = (sim->player_vel.z - prev_vel.z) / fdt;

    sim->player_pos.x = rts_clampf(sim->player_pos.x + sim->player_vel.x * fdt, -sim->arena_half_m + 1.5f, sim->arena_half_m - 1.5f);
    sim->player_pos.z = rts_clampf(sim->player_pos.z + sim->player_vel.z * fdt, -sim->arena_half_m + 1.5f, sim->arena_half_m - 1.5f);

    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
        if (sim->input_buffer_ttl_s[a] > 0.0f && can_execute_action(sim, (RtsTacticalActionId)a)) {
            if (execute_action(sim, (RtsTacticalActionId)a, in)) {
                sim->last_tick.last_action_accepted = true;
                uint32_t rtick = sim->input_buffer_ready_tick[a] > 0u ? sim->input_buffer_ready_tick[a] : sim->tick_index;
                sim->last_tick.visual_response_delay_frames = (sim->tick_index - rtick) + 1u;
                sim->input_buffer_ttl_s[a] = 0.0f;
            }
        }
        if (sim->input_buffer_ttl_s[a] > 0.0f) {
            sim->input_buffer_ttl_s[a] = fmaxf(0.0f, sim->input_buffer_ttl_s[a] - fdt);
        }
    }

    for (int i = 0; i < RTS_TAC_MAX_PICKUPS; ++i) {
        RtsTacPickup *pk = &sim->pickups[i];
        if (!pk->active) continue;
        pk->life_s -= fdt;
        if (pk->life_s <= 0.0f) {
            pk->active = false;
            continue;
        }
        float dx = pk->pos.x - sim->player_pos.x;
        float dz = pk->pos.z - sim->player_pos.z;
        float collect_r = (pk->radius_m + sim->cfg.player_sprite_radius_m) * sim->cfg.attack_hitbox_generosity;
        if (sqrtf(dx * dx + dz * dz) <= collect_r) {
            pk->active = false;
            sim->tactical_energy = rts_clampf(sim->tactical_energy + pk->energy_bonus, 0.0f, sim->max_tactical_energy);
            sim->exo_shield = rts_clampf(sim->exo_shield + pk->shield_bonus, 0.0f, sim->max_exo_shield);
            sim->score += 150.0f;
            sim->last_tick.reward_delta += 150.0f;
            sim->last_micro_reward_s = sim->sim_time_s;
            spawn_particles(sim, pk->pos, (RtsVec3f){0, 1, 0}, 12, (RtsVec4f){1.0f, 0.86f, 0.30f, 1.0f}, 9.0f, 0.25f);
            trigger_sfx(sim, "assets/audio/hologlobe_ping.wav", pk->pos, 0.85f);
            add_popup(sim, "RESUPPLY +32 ENERGY!", (RtsVec4f){1.0f, 0.88f, 0.32f, 1.0f}, 1.0f);
            record_juice_event(sim, RTS_JUICE_ANIM_SQUASH | RTS_JUICE_PARTICLES | RTS_JUICE_AUDIO | RTS_JUICE_UI_POP, false);
        }
    }

    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        RtsTacEnemy *e = &sim->enemies[i];
        if (!e->active) continue;
        if (e->hit_flash_s > 0.0f) e->hit_flash_s = fmaxf(0.0f, e->hit_flash_s - fdt);
        if (e->squash_timer_s > 0.0f) e->squash_timer_s = fmaxf(0.0f, e->squash_timer_s - fdt);
        if (e->emp_primed_timer_s > 0.0f) e->emp_primed_timer_s = fmaxf(0.0f, e->emp_primed_timer_s - fdt);

        if (e->spawn_warmup_s > 0.0f) {
            e->spawn_warmup_s = fmaxf(0.0f, e->spawn_warmup_s - fdt);
            continue;
        }

        float dx = sim->player_pos.x - e->pos.x;
        float dz = sim->player_pos.z - e->pos.z;
        float dist = sqrtf(dx * dx + dz * dz);
        float inv_d = dist > 1e-4f ? (1.0f / dist) : 0.0f;
        float nx = dx * inv_d;
        float nz = dz * inv_d;

        float pref_dist = (e->archetype == RTS_ENEMY_INTERCEPTOR) ? 12.0f : ((e->archetype == RTS_ENEMY_SHIELD_CRUISER) ? 17.0f : 23.0f);
        float spd = (e->archetype == RTS_ENEMY_INTERCEPTOR) ? 11.5f : ((e->archetype == RTS_ENEMY_SHIELD_CRUISER) ? 6.0f : 4.2f);
        float strafe_sign = (e->id % 2u == 0u) ? 1.0f : -1.0f;
        float approach = (dist > pref_dist + 2.0f) ? 1.0f : ((dist < pref_dist - 3.0f) ? -0.6f : 0.0f);
        e->vel.x = (nx * approach - nz * 0.55f * strafe_sign) * spd;
        e->vel.z = (nz * approach + nx * 0.55f * strafe_sign) * spd;
        e->pos.x = rts_clampf(e->pos.x + e->vel.x * fdt, -sim->arena_half_m + 2.0f, sim->arena_half_m - 2.0f);
        e->pos.z = rts_clampf(e->pos.z + e->vel.z * fdt, -sim->arena_half_m + 2.0f, sim->arena_half_m - 2.0f);

        if (e->telegraph_timer_s > 0.0f) {
            e->telegraph_timer_s -= fdt;
            if (e->telegraph_timer_s <= 0.0f) {
                RtsVec3f fire_dir = e->telegraph_aim_dir;
                if (e->archetype == RTS_ENEMY_INTERCEPTOR) {
                    spawn_projectile(sim, e->pos, fire_dir, 30.0f, 24.0f, 0.50f, false, false);
                    e->fire_cooldown_s = (float)(1.15 / sim->cfg.enemy_fire_rate_scale);
                } else if (e->archetype == RTS_ENEMY_SHIELD_CRUISER) {
                    for (int k = 0; k < 2; ++k) {
                        float spread = (k == 0 ? -0.09f : 0.09f);
                        float cs = cosf(spread), sn = sinf(spread);
                        RtsVec3f sd = {fire_dir.x * cs - fire_dir.z * sn, fire_dir.y, fire_dir.x * sn + fire_dir.z * cs};
                        spawn_projectile(sim, e->pos, sd, 26.5f, 26.0f, 0.56f, false, false);
                    }
                    e->fire_cooldown_s = (float)(1.85 / sim->cfg.enemy_fire_rate_scale);
                } else {
                    spawn_projectile(sim, e->pos, fire_dir, 36.0f, 42.0f, 0.72f, false, false);
                    e->fire_cooldown_s = (float)(2.05 / sim->cfg.enemy_fire_rate_scale);
                }
            }
        } else {
            e->fire_cooldown_s -= fdt;
            if (e->fire_cooldown_s <= 0.0f) {
                /* Mild velocity lead prediction so standing still or moving predictably gets punished while active dodging works! */
                float lead_t = rts_clampf(dist / 36.0f, 0.0f, 0.45f);
                float tx = sim->player_pos.x + sim->player_vel.x * lead_t * 0.55f - e->pos.x;
                float tz = sim->player_pos.z + sim->player_vel.z * lead_t * 0.55f - e->pos.z;
                float ty = sim->player_pos.y + 1.1f - e->pos.y;
                float al = sqrtf(tx * tx + ty * ty + tz * tz);
                RtsVec3f aim = al > 1e-4f ? (RtsVec3f){tx / al, ty / al, tz / al} : (RtsVec3f){nx, 0.0f, nz};
                e->telegraph_aim_dir = aim;
                if (sim->cfg.telegraph_lead_s > 0.0) {
                    float lead = (float)sim->cfg.telegraph_lead_s + (e->archetype == RTS_ENEMY_SIEGE_WALKER ? 0.06f : 0.0f);
                    e->telegraph_timer_s = lead;
                    e->telegraph_total_s = lead;
                    trigger_sfx(sim, "assets/audio/alarm.wav", e->pos, 0.35f);
                } else {
                    spawn_projectile(sim, e->pos, aim, 25.0f, 18.0f, 0.55f, false, false);
                    e->fire_cooldown_s = (float)(1.15 / sim->cfg.enemy_fire_rate_scale);
                }
            }
        }
    }

    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        RtsTacProjectile *pr = &sim->projectiles[i];
        if (!pr->active) continue;
        pr->life_s -= fdt;
        if (pr->life_s <= 0.0f) {
            pr->active = false;
            continue;
        }
        pr->pos.x += pr->vel.x * fdt;
        pr->pos.y += pr->vel.y * fdt;
        pr->pos.z += pr->vel.z * fdt;

        if (fabsf(pr->pos.x) > sim->arena_half_m + 6.0f || fabsf(pr->pos.z) > sim->arena_half_m + 6.0f) {
            pr->active = false;
            continue;
        }

        if (pr->from_player) {
            for (int e_i = 0; e_i < RTS_TAC_MAX_ENEMIES; ++e_i) {
                RtsTacEnemy *e = &sim->enemies[e_i];
                if (!e->active || e->spawn_warmup_s > 0.0f) continue;
                float dx = pr->pos.x - e->pos.x;
                float dz = pr->pos.z - e->pos.z;
                float hit_r = e->radius_m * sim->cfg.attack_hitbox_generosity + pr->radius_m;
                if (sqrtf(dx * dx + dz * dz) <= hit_r) {
                    pr->active = false;
                    damage_enemy(sim, e, pr->damage, true, false);
                    break;
                }
            }
        } else {
            float dx = pr->pos.x - sim->player_pos.x;
            float dz = pr->pos.z - sim->player_pos.z;
            float dist = sqrtf(dx * dx + dz * dz);
            float hurt_r = sim->cfg.player_hurt_radius_m + pr->radius_m * 0.55f;
            if (dist <= hurt_r) {
                pr->active = false;
                float pl = sqrtf(pr->vel.x * pr->vel.x + pr->vel.z * pr->vel.z);
                RtsVec3f hdir = pl > 1e-4f ? (RtsVec3f){pr->vel.x / pl, 0.3f, pr->vel.z / pl} : (RtsVec3f){0, 1, 0};
                damage_player(sim, pr->damage, hdir);
            } else if (!pr->near_miss_counted && dist > hurt_r && dist <= hurt_r * 1.50f) {
                pr->near_miss_counted = true;
                sim->near_misses_total++;
                sim->last_tick.near_misses_this_tick++;
                if (sim->cfg.juice_enabled) {
                    sim->tactical_energy = rts_clampf(sim->tactical_energy + 15.0f, 0.0f, sim->max_tactical_energy);
                    sim->score += 40.0f;
                    sim->last_tick.reward_delta += 40.0f;
                    sim->last_micro_reward_s = sim->sim_time_s;
                    add_popup(sim, "NEAR-MISS GRAZE! +15", (RtsVec4f){0.28f, 1.0f, 0.88f, 1.0f}, 0.85f);
                    trigger_sfx(sim, "assets/audio/hologlobe_ping.wav", sim->player_pos, 0.55f);
                    record_juice_event(sim, RTS_JUICE_AUDIO | RTS_JUICE_UI_POP | RTS_JUICE_PARTICLES, false);
                }
            }
        }
    }

    for (int i = 0; i < RTS_TAC_MAX_PARTICLES; ++i) {
        RtsTacParticle *pt = &sim->particles[i];
        if (!pt->active) continue;
        pt->life_s -= fdt;
        if (pt->life_s <= 0.0f) {
            pt->active = false;
            continue;
        }
        float drag = expf(-3.4f * fdt);
        pt->vel.x *= drag;
        pt->vel.y = pt->vel.y * drag - 6.5f * fdt;
        pt->vel.z *= drag;
        pt->pos.x += pt->vel.x * fdt;
        pt->pos.y = fmaxf(0.05f, pt->pos.y + pt->vel.y * fdt);
        pt->pos.z += pt->vel.z * fdt;
    }

    step_wave_director(sim, dt_s);
    if (sim->active_run && sim->sim_time_s >= sim->max_duration_s) {
        sim->active_run = false;
        sim->victory = (sim->enemies_killed >= 8u);
        if (sim->victory) {
            sim->waves_cleared++;
            sim->score += 600.0f;
            sim->last_tick.reward_delta += 600.0f;
            sim->last_meso_reward_s = sim->sim_time_s;
            add_popup(sim, "SECTOR SECURED - VICTORY!", (RtsVec4f){0.30f, 1.0f, 0.78f, 1.0f}, 3.0f);
        }
    }
}

void rts_tactical_sync_to_world(const RtsTacticalSim *sim, RtsWorld *world) {
    if (!sim || !world) return;
    world->commander.local_pos = rts_v3d((double)sim->player_pos.x, (double)sim->player_pos.y + 1.8, (double)sim->player_pos.z);
    world->commander.local_vel = rts_v3d((double)sim->player_vel.x, (double)sim->player_vel.y, (double)sim->player_vel.z);
    world->commander.yaw_rad = (double)sim->player_yaw;
    world->commander.pitch_rad = (double)sim->player_pitch;
    world->commander.health = sim->health;
    world->commander.max_health = sim->max_health;
    world->commander.exo_shield = sim->exo_shield;
    world->commander.max_exo_shield = sim->max_exo_shield;
    world->commander.alive = !sim->game_over;
    if (sim->game_over) {
        world->match_state = RTS_MATCH_DEFEAT_IRONMAN;
    } else if (sim->victory) {
        world->match_state = RTS_MATCH_VICTORY;
    } else {
        world->match_state = RTS_MATCH_ACTIVE;
    }
}

uint32_t rts_tactical_active_enemy_count(const RtsTacticalSim *sim) {
    if (!sim) return 0u;
    uint32_t c = 0u;
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (sim->enemies[i].active) c++;
    }
    return c;
}

uint32_t rts_tactical_active_particle_count(const RtsTacticalSim *sim) {
    if (!sim) return 0u;
    uint32_t c = 0u;
    for (int i = 0; i < RTS_TAC_MAX_PARTICLES; ++i) {
        if (sim->particles[i].active) c++;
    }
    return c;
}

float rts_tactical_nearest_threat_pw(const RtsTacticalSim *sim) {
    if (!sim) return 999.0f;
    float best = 999.0f;
    float pw = fmaxf(0.5f, sim->cfg.player_sprite_radius_m * 2.0f);
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (!sim->enemies[i].active) continue;
        float dx = sim->enemies[i].pos.x - sim->player_pos.x;
        float dz = sim->enemies[i].pos.z - sim->player_pos.z;
        float d = sqrtf(dx * dx + dz * dz) / pw;
        if (d < best) best = d;
    }
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        if (!sim->projectiles[i].active || sim->projectiles[i].from_player) continue;
        float dx = sim->projectiles[i].pos.x - sim->player_pos.x;
        float dz = sim->projectiles[i].pos.z - sim->player_pos.z;
        float d = sqrtf(dx * dx + dz * dz) / pw;
        if (d < best) best = d;
    }
    return best;
}

float rts_tactical_compute_tension(const RtsTacticalSim *sim, float recent_input_rate_hz) {
    if (!sim) return 0.0f;
    uint32_t enemies = rts_tactical_active_enemy_count(sim);
    uint32_t host_proj = 0u;
    uint32_t telegraphs = 0u;
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (sim->enemies[i].active && sim->enemies[i].telegraph_timer_s > 0.0f) telegraphs++;
    }
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        if (sim->projectiles[i].active && !sim->projectiles[i].from_player) host_proj++;
    }
    float threat_density = rts_clampf(((float)enemies * 0.14f + (float)host_proj * 0.08f + (float)telegraphs * 0.18f), 0.0f, 1.0f);
    if (sim->wave_phase == RTS_WAVE_BREATHER && enemies <= 1u) {
        threat_density *= 0.25f;
    }
    float input_norm = rts_clampf(recent_input_rate_hz / 4.5f, 0.0f, 1.0f);
    float total_hp = sim->health + sim->exo_shield;
    float max_total = sim->max_health + sim->max_exo_shield;
    float hp_loss = 1.0f - rts_clampf(total_hp / fmaxf(1.0f, max_total), 0.0f, 1.0f);
    return rts_clampf(0.45f * threat_density + 0.25f * input_norm + 0.30f * hp_loss, 0.0f, 1.0f);
}

float rts_tactical_completion_score(const RtsTacticalSim *sim) {
    if (!sim) return 0.0f;
    float surv = rts_clampf((float)(sim->sim_time_s / fmax(1.0, sim->max_duration_s)), 0.0f, 1.0f);
    float kills = rts_clampf((float)sim->enemies_killed / 18.0f, 0.0f, 1.0f);
    float waves = rts_clampf((float)sim->waves_cleared / 3.0f, 0.0f, 1.0f);
    return rts_clampf(0.32f * surv + 0.48f * kills + 0.20f * waves, 0.0f, 1.0f);
}
