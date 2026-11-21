/* SPDX-License-Identifier: MIT */
#include "rts/fun_eval.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HUMAN_DELAY_RING_SIZE 32

typedef struct BotObsEnemy {
    RtsVec3f pos;
    RtsEnemyArchetype archetype;
    float shield;
    float emp_primed;
    float telegraph;
    bool active;
} BotObsEnemy;

typedef struct BotObsProj {
    RtsVec3f pos;
    RtsVec3f vel;
    bool active;
} BotObsProj;

typedef struct BotObsSnapshot {
    RtsVec3f player_pos;
    float health;
    float exo_shield;
    float tactical_energy;
    float weapon_heat;
    float cooldown_s[RTS_ACT_COUNT];
    RtsWavePhase wave_phase;
    BotObsEnemy enemies[RTS_TAC_MAX_ENEMIES];
    BotObsProj projs[RTS_TAC_MAX_PROJECTILES];
    RtsVec3f pickup_pos;
    bool has_pickup;
    bool valid;
} BotObsSnapshot;

static double bot_rand01(uint64_t *state) {
    uint64_t x = *state;
    if (x == 0u) x = 0xD1B54A32D192ED03ull;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    *state = x;
    return (double)((x * 2685821657736338717ull) >> 11) * (1.0 / 9007199254740992.0);
}

static void capture_obs(const RtsTacticalSim *sim, BotObsSnapshot *obs) {
    memset(obs, 0, sizeof(*obs));
    obs->player_pos = sim->player_pos;
    obs->health = sim->health;
    obs->exo_shield = sim->exo_shield;
    obs->tactical_energy = sim->tactical_energy;
    obs->weapon_heat = sim->weapon_heat;
    memcpy(obs->cooldown_s, sim->cooldown_s, sizeof(obs->cooldown_s));
    obs->wave_phase = sim->wave_phase;
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (!sim->enemies[i].active || sim->enemies[i].spawn_warmup_s > 0.0f) continue;
        obs->enemies[i].pos = sim->enemies[i].pos;
        obs->enemies[i].archetype = sim->enemies[i].archetype;
        obs->enemies[i].shield = sim->enemies[i].shield;
        obs->enemies[i].emp_primed = sim->enemies[i].emp_primed_timer_s;
        obs->enemies[i].telegraph = sim->enemies[i].telegraph_timer_s;
        obs->enemies[i].active = true;
    }
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        if (!sim->projectiles[i].active || sim->projectiles[i].from_player) continue;
        obs->projs[i].pos = sim->projectiles[i].pos;
        obs->projs[i].vel = sim->projectiles[i].vel;
        obs->projs[i].active = true;
    }
    for (int i = 0; i < RTS_TAC_MAX_PICKUPS; ++i) {
        if (sim->pickups[i].active) {
            obs->pickup_pos = sim->pickups[i].pos;
            obs->has_pickup = true;
            break;
        }
    }
    obs->valid = true;
}

static void decide_from_obs(RtsBotPersona persona, const BotObsSnapshot *obs, uint64_t *rng, uint32_t tick, RtsTacticalInput *out) {
    memset(out, 0, sizeof(*out));

    if (persona == RTS_BOT_NOVICE) {
        /* Persona A: Random valid inputs at ~3 Hz with zero spatial awareness */
        double r_dir = bot_rand01(rng);
        if ((tick % 20u) < 10u) {
            out->move_x = (r_dir < 0.33) ? -1.0f : ((r_dir < 0.66) ? 1.0f : 0.0f);
            out->move_z = (bot_rand01(rng) < 0.5) ? 0.6f : -0.4f;
        }
        out->aim_yaw = (float)(bot_rand01(rng) * 2.0 * RTS_PI);
        if (bot_rand01(rng) < 0.05) { /* ~3 Hz discrete triggers */
            int act = (int)(bot_rand01(rng) * (double)RTS_ACT_COUNT) % RTS_ACT_COUNT;
            out->action_pressed[act] = true;
        }
        return;
    }

    /* Locate nearest active enemy & closest incoming threat projectile */
    int best_e = -1;
    float best_e_dist = 9999.0f;
    int shielded_close = 0;
    int active_enemies = 0;
    for (int i = 0; i < RTS_TAC_MAX_ENEMIES; ++i) {
        if (!obs->enemies[i].active) continue;
        active_enemies++;
        float dx = obs->enemies[i].pos.x - obs->player_pos.x;
        float dz = obs->enemies[i].pos.z - obs->player_pos.z;
        float d = sqrtf(dx * dx + dz * dz);
        if (d < 26.0f && (obs->enemies[i].shield > 15.0f || obs->enemies[i].telegraph > 0.0f)) {
            shielded_close++;
        }
        if (d < best_e_dist) {
            best_e_dist = d;
            best_e = i;
        }
    }

    if (best_e >= 0) {
        float dx = obs->enemies[best_e].pos.x - obs->player_pos.x;
        float dz = obs->enemies[best_e].pos.z - obs->player_pos.z;
        out->aim_yaw = atan2f(dx, -dz);
    }

    if (persona == RTS_BOT_SPAM) {
        /* Degenerate SpamBot: aims at target, walks slowly, spams ONLY Primary Plasma Lance */
        out->move_z = (best_e_dist > 14.0f) ? 0.6f : 0.0f;
        out->move_x = 0.25f;
        out->action_pressed[RTS_ACT_PLASMA_LANCE] = true;
        return;
    }

    bool is_oracle = (persona == RTS_BOT_ORACLE);

    /* Threat evasion from incoming enemy projectiles */
    float evade_x = 0.0f;
    float evade_z = 0.0f;
    bool urgent_threat = false;
    float closest_proj_d = 999.0f;
    for (int i = 0; i < RTS_TAC_MAX_PROJECTILES; ++i) {
        if (!obs->projs[i].active) continue;
        float rx = obs->player_pos.x - obs->projs[i].pos.x;
        float rz = obs->player_pos.z - obs->projs[i].pos.z;
        float d = sqrtf(rx * rx + rz * rz);
        float vx = obs->projs[i].vel.x;
        float vz = obs->projs[i].vel.z;
        float vlen = sqrtf(vx * vx + vz * vz);
        if (vlen < 0.1f) continue;
        float dot = (rx * vx + rz * vz) / vlen;
        /* Human-Proxy notices threats inside shorter peripheral window; Oracle tracks all trajectories cleanly */
        float detect_r = is_oracle ? 16.0f : 7.8f;
        if (dot > 0.0f && d < detect_r) {
            float cross = rx * vz - rz * vx;
            float sign = (cross >= 0.0f) ? 1.0f : -1.0f;
            float px = -vz / vlen * sign;
            float pz =  vx / vlen * sign;
            float w = 1.0f / fmaxf(1.5f, d);
            evade_x += px * w;
            evade_z += pz * w;
            if (d < closest_proj_d) closest_proj_d = d;
            if (d < (is_oracle ? 9.5f : 5.4f)) urgent_threat = true;
        }
    }

    /* Convert world wish vector into camera-local (move_x, move_z) */
    float wx = 0.0f, wz = 0.0f;
    if (urgent_threat || closest_proj_d < (is_oracle ? 13.0f : 7.0f)) {
        wx = evade_x;
        wz = evade_z;
    } else if (obs->has_pickup && (obs->wave_phase == RTS_WAVE_BREATHER || obs->tactical_energy < 35.0f)) {
        wx = obs->pickup_pos.x - obs->player_pos.x;
        wz = obs->pickup_pos.z - obs->player_pos.z;
    } else if (best_e >= 0) {
        float dx = obs->enemies[best_e].pos.x - obs->player_pos.x;
        float dz = obs->enemies[best_e].pos.z - obs->player_pos.z;
        float d = fmaxf(0.1f, best_e_dist);
        float strafe_dir = ((tick / 90u) % 2u == 0u) ? 1.0f : -1.0f;
        float radial = (d > 19.0f) ? 0.55f : ((d < 11.0f) ? -0.55f : 0.0f);
        wx = (dx / d) * radial - (dz / d) * 0.85f * strafe_dir;
        wz = (dz / d) * radial + (dx / d) * 0.85f * strafe_dir;
    }
    /* Gentle arena centering pull to avoid corner traps */
    if (fabsf(obs->player_pos.x) > 30.0f) wx -= obs->player_pos.x * 0.05f;
    if (fabsf(obs->player_pos.z) > 30.0f) wz -= obs->player_pos.z * 0.05f;

    float wlen = sqrtf(wx * wx + wz * wz);
    if (wlen > 1e-4f) {
        wx /= wlen;
        wz /= wlen;
        float cy = cosf(out->aim_yaw);
        float sy = sinf(out->aim_yaw);
        out->move_x = wx * cy + wz * sy;
        out->move_z = wx * sy - wz * cy;
    }

    /* Realistic early-press window for Human-Proxy (40-90ms pre-ready) to test Input Buffering R_drop */
    float early_window = is_oracle ? 0.0f : 0.085f;
    if (!is_oracle && bot_rand01(rng) < 0.03) {
        out->jump_pressed = true;
    }

    /* Tactical Decision Hierarchy (5 distinct voluntary choices with situational triggers) */
    if (urgent_threat && obs->cooldown_s[RTS_ACT_THRUSTER_DASH] <= early_window) {
        if (is_oracle || bot_rand01(rng) < 0.55) {
            out->action_pressed[RTS_ACT_THRUSTER_DASH] = true;
        }
    }
    if ((obs->exo_shield < 28.0f || obs->health < 68.0f) && obs->cooldown_s[RTS_ACT_FIELD_REPAIR] <= early_window) {
        if (is_oracle || bot_rand01(rng) < 0.65) {
            out->action_pressed[RTS_ACT_FIELD_REPAIR] = true;
        }
    }
    if (shielded_close > 0 && obs->cooldown_s[RTS_ACT_EMP_PULSE] <= early_window) {
        if (is_oracle || bot_rand01(rng) < 0.72) {
            out->action_pressed[RTS_ACT_EMP_PULSE] = true;
        }
    }
    if (obs->tactical_energy >= 25.0f && active_enemies >= 2 && obs->cooldown_s[RTS_ACT_FLEET_STRIKE] <= early_window) {
        if (is_oracle || bot_rand01(rng) < 0.75) {
            out->action_pressed[RTS_ACT_FLEET_STRIKE] = true;
        }
    }

    /* Primary Plasma Lance: Oracle manages heat & prioritizes EMP-primed shots; Human fires steadily with minor aim jitter */
    if (best_e >= 0 && obs->cooldown_s[RTS_ACT_PLASMA_LANCE] <= early_window) {
        bool hot_guard = is_oracle ? (obs->weapon_heat < 0.80f || obs->enemies[best_e].emp_primed > 0.0f) : (obs->weapon_heat < 0.90f);
        if (hot_guard) {
            if (!is_oracle) {
                /* Human aim micro-jitter +-0.09 rad */
                out->aim_yaw += (float)((bot_rand01(rng) - 0.5) * 0.18);
            }
            if (is_oracle || (tick % 3u != 0u)) {
                out->action_pressed[RTS_ACT_PLASMA_LANCE] = true;
            }
        }
    }
}

void rts_fun_bot_decide(RtsBotPersona persona, const RtsTacticalSim *sim, uint64_t *bot_rng, RtsTacticalInput *out_in) {
    BotObsSnapshot obs;
    capture_obs(sim, &obs);
    decide_from_obs(persona, &obs, bot_rng, sim->tick_index, out_in);
}

static void write_telemetry_jsonl(FILE *fp, const RtsTacticalSim *sim, uint32_t tick, float tension) {
    if (!fp) return;
    double ts_ms = sim->run_start_ms + sim->sim_time_s * 1000.0;
    float shake_mag = sqrtf(sim->camera_shake_offset.x * sim->camera_shake_offset.x +
                            sim->camera_shake_offset.y * sim->camera_shake_offset.y);
    const char *easing = sim->cfg.nonlinear_motion ? "nonlinear" : "linear";
    fprintf(fp,
            "{\"tick\":%u,\"lifecycle\":{\"boot_ms\":%.1f,\"first_interactive_ms\":%.1f,\"run_start_ms\":%.1f,\"game_over_ms\":%.1f,\"restart_ready_ms\":%.1f},"
            "\"input\":{\"timestamp_ms\":%.1f,\"action_id\":%d,\"accepted\":%s,\"visual_response_frame\":%u},"
            "\"entity\":{\"pos\":[%.2f,%.2f],\"vel\":[%.2f,%.2f],\"accel\":[%.2f,%.2f],\"health\":%.1f,\"shield\":%.1f,"
            "\"sprite_box\":[%.2f,%.2f],\"hurtbox\":[%.2f,%.2f],\"active_enemies\":%u,\"d_min_pw\":%.2f},"
            "\"juice\":{\"particles\":%u,\"shake_mag\":%.3f,\"hitstop_ticks\":%u,\"audio_triggers\":%u,\"easing\":\"%s\"},"
            "\"decision\":{\"choices_offered\":%d,\"choice_selected\":%d,\"delta_r\":%.1f},\"tension\":%.3f}\n",
            tick, sim->boot_ms, sim->first_interactive_ms, sim->run_start_ms, sim->game_over_ms, sim->restart_ready_ms,
            ts_ms,
            sim->last_tick.any_action_attempted ? (int)sim->last_tick.last_action : -1,
            sim->last_tick.last_action_accepted ? "true" : "false",
            sim->last_tick.visual_response_delay_frames,
            sim->player_pos.x, sim->player_pos.z,
            sim->player_vel.x, sim->player_vel.z,
            sim->player_accel.x, sim->player_accel.z,
            sim->health, sim->exo_shield,
            sim->cfg.player_sprite_radius_m * 2.0f, sim->cfg.player_sprite_radius_m * 2.0f,
            sim->cfg.player_hurt_radius_m * 2.0f, sim->cfg.player_hurt_radius_m * 2.0f,
            rts_tactical_active_enemy_count(sim),
            rts_tactical_nearest_threat_pw(sim),
            rts_tactical_active_particle_count(sim),
            shake_mag,
            sim->hitstop_ticks_total,
            sim->last_tick.audio_triggers_this_tick,
            easing,
            RTS_ACT_COUNT,
            sim->last_tick.last_action_accepted ? (int)sim->last_tick.last_action : -1,
            sim->last_tick.reward_delta,
            tension);
}

static double clamp01(double v) {
    if (v < 0.0) return 0.0;
    if (v > 1.0) return 1.0;
    return v;
}

static double range_score(double v, double lo, double hi, double soft_margin) {
    if (v >= lo && v <= hi) return 1.0;
    if (v < lo) return clamp01(1.0 - (lo - v) / fmax(1e-6, soft_margin));
    return clamp01(1.0 - (v - hi) / fmax(1e-6, soft_margin));
}

bool rts_fun_run_audit(const RtsFeelConfig *cfg,
                       uint64_t base_seed,
                       int episodes_per_persona,
                       const char *telemetry_jsonl_path,
                       double measured_wcag_contrast,
                       RtsFunAuditReport *out_report) {
    if (!cfg || !out_report || episodes_per_persona < 1) return false;
    memset(out_report, 0, sizeof(*out_report));
    out_report->episodes_per_persona = episodes_per_persona;
    out_report->base_seed = base_seed;
    out_report->baseline_mode = !cfg->juice_enabled && !cfg->wave_director_enabled;

    FILE *tfp = NULL;
    if (telemetry_jsonl_path && telemetry_jsonl_path[0] != '\0') {
        tfp = fopen(telemetry_jsonl_path, "w");
    }

    double w_scores[RTS_BOT_COUNT][128];
    int n_ep = episodes_per_persona > 128 ? 128 : episodes_per_persona;

    /* Aggregate telemetry accumulators (from Human-Proxy & Oracle) */
    uint64_t latency_samples = 0u;
    double latency_frames_sum = 0.0;
    uint64_t human_attempts = 0u;
    uint64_t human_drops_near_ready = 0u;
    uint64_t core_ch_sum = 0u, core_ev_sum = 0u;
    uint64_t heavy_ch_sum = 0u, heavy_ev_sum = 0u;
    uint64_t action_counts[RTS_ACT_COUNT];
    memset(action_counts, 0, sizeof(action_counts));
    uint64_t nonlinear_ticks = 0u, moving_ticks = 0u;

    double tension_samples[8192];
    int tension_count = 0;
    double max_sustained_hi_t = 0.0;
    double near_misses_sum = 0.0;
    double human_minutes_sum = 0.0;
    double micro_interval_sum = 0.0;
    int micro_interval_cnt = 0;
    double meso_interval_sum = 0.0;
    int meso_interval_cnt = 0;

    const double dt_s = 1.0 / 60.0;
    BotObsSnapshot *delay_ring = (BotObsSnapshot *)calloc(HUMAN_DELAY_RING_SIZE, sizeof(BotObsSnapshot));
    if (!delay_ring) {
        if (tfp) fclose(tfp);
        return false;
    }

    for (int p = 0; p < RTS_BOT_COUNT; ++p) {
        RtsBotPersona persona = (RtsBotPersona)p;
        for (int ep = 0; ep < n_ep; ++ep) {
            uint64_t ep_seed = base_seed + (uint64_t)ep * 9973u + 17u;
            uint64_t bot_rng = ep_seed ^ ((uint64_t)(p + 1) * 0x517CC1B727220A95ull);

            RtsTacticalSim sim;
            rts_tactical_init(&sim, cfg, ep_seed, NULL);
            memset(delay_ring, 0, HUMAN_DELAY_RING_SIZE * sizeof(BotObsSnapshot));

            bool log_this_run = (tfp != NULL && persona == RTS_BOT_HUMAN_PROXY && ep == 0);
            int recent_inputs_sec = 0;
            double cur_sustained_hi = 0.0;
            double prev_micro_s = 0.0;
            double prev_meso_s = 0.0;

            for (uint32_t tick = 0; tick < 3600u && sim.active_run; ++tick) {
                capture_obs(&sim, &delay_ring[tick % HUMAN_DELAY_RING_SIZE]);

                const BotObsSnapshot *use_obs = &delay_ring[tick % HUMAN_DELAY_RING_SIZE];
                if (persona == RTS_BOT_HUMAN_PROXY) {
                    /* Simulated human reaction delay: 200ms (12 frames) +- 35ms (+-2 frames) jitter */
                    int jitter = (int)(bot_rand01(&bot_rng) * 5.0) - 2;
                    int delay_frames = 12 + jitter;
                    if (delay_frames < 10) delay_frames = 10;
                    if (delay_frames > 14) delay_frames = 14;
                    if ((int)tick >= delay_frames) {
                        use_obs = &delay_ring[(tick - (uint32_t)delay_frames) % HUMAN_DELAY_RING_SIZE];
                    } else {
                        use_obs = &delay_ring[0];
                    }
                }

                RtsTacticalInput in;
                decide_from_obs(persona, use_obs, &bot_rng, tick, &in);
                rts_tactical_step(&sim, &in, dt_s);

                if (persona == RTS_BOT_HUMAN_PROXY) {
                    if (sim.last_tick.any_action_attempted) {
                        human_attempts++;
                        recent_inputs_sec++;
                        if (sim.last_tick.last_action_dropped_near_ready) {
                            human_drops_near_ready++;
                        }
                    }
                    if (sim.last_tick.visual_response_delay_frames > 0u) {
                        latency_frames_sum += (double)sim.last_tick.visual_response_delay_frames;
                        latency_samples++;
                    }
                    float spd = sqrtf(sim.player_vel.x * sim.player_vel.x + sim.player_vel.z * sim.player_vel.z);
                    float acc = sqrtf(sim.player_accel.x * sim.player_accel.x + sim.player_accel.z * sim.player_accel.z);
                    if (spd > 0.5f) {
                        moving_ticks++;
                        if (acc > 1.0f && acc < 350.0f) {
                            nonlinear_ticks++;
                        }
                    }
                    core_ch_sum += sim.last_tick.juice_channels_core_sum;
                    core_ev_sum += sim.last_tick.juice_core_events;
                    heavy_ch_sum += sim.last_tick.juice_channels_heavy_sum;
                    heavy_ev_sum += sim.last_tick.juice_heavy_events;

                    if (sim.last_micro_reward_s > prev_micro_s + 0.05) {
                        micro_interval_sum += (sim.last_micro_reward_s - prev_micro_s);
                        micro_interval_cnt++;
                        prev_micro_s = sim.last_micro_reward_s;
                    }
                    if (sim.last_meso_reward_s > prev_meso_s + 0.05) {
                        meso_interval_sum += (sim.last_meso_reward_s - prev_meso_s);
                        meso_interval_cnt++;
                        prev_meso_s = sim.last_meso_reward_s;
                    }
                }

                if (persona == RTS_BOT_HUMAN_PROXY || persona == RTS_BOT_ORACLE) {
                    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
                        if (sim.last_tick.action_executed[a]) {
                            action_counts[a]++;
                        }
                    }
                }

                /* Sample Tension Index T(t) at 1 Hz on Human-Proxy */
                float t_now = rts_tactical_compute_tension(&sim, (float)recent_inputs_sec);
                if (log_this_run) {
                    write_telemetry_jsonl(tfp, &sim, tick, t_now);
                }
                if (persona == RTS_BOT_HUMAN_PROXY && ((tick + 1u) % 60u == 0u)) {
                    if (tension_count < 8192) {
                        tension_samples[tension_count++] = (double)t_now;
                    }
                    if (t_now > 0.90f) {
                        cur_sustained_hi += 1.0;
                        if (cur_sustained_hi > max_sustained_hi_t) max_sustained_hi_t = cur_sustained_hi;
                    } else {
                        cur_sustained_hi = 0.0;
                    }
                    recent_inputs_sec = 0;
                }
            }

            w_scores[p][ep] = (double)rts_tactical_completion_score(&sim);
            if (persona == RTS_BOT_HUMAN_PROXY) {
                near_misses_sum += (double)sim.near_misses_total;
                human_minutes_sum += fmax(0.05, sim.sim_time_s / 60.0);
            }
        }
    }

    free(delay_ring);
    if (tfp) fclose(tfp);

    /* Compute persona win/completion means & Human std-dev */
    double means[RTS_BOT_COUNT] = {0};
    for (int p = 0; p < RTS_BOT_COUNT; ++p) {
        double s = 0.0;
        for (int ep = 0; ep < n_ep; ++ep) s += w_scores[p][ep];
        means[p] = s / (double)n_ep;
    }
    double var_h = 0.0;
    for (int ep = 0; ep < n_ep; ++ep) {
        double d = w_scores[RTS_BOT_HUMAN_PROXY][ep] - means[RTS_BOT_HUMAN_PROXY];
        var_h += d * d;
    }
    double std_h = sqrt(var_h / (double)n_ep);

    /* --- Pillar 1: Kinesthetics & Feel (M_feel) --- */
    double avg_lat_frames = latency_samples > 0u ? (latency_frames_sum / (double)latency_samples) : 1.0;
    out_report->latency_ms = avg_lat_frames * (1000.0 / 60.0);
    out_report->input_drop_rate = human_attempts > 0u ? ((double)human_drops_near_ready / (double)human_attempts) : 0.0;
    out_report->motion_nonlinear = cfg->nonlinear_motion && (moving_ticks > 0u && ((double)nonlinear_ticks / (double)moving_ticks) > 0.08);
    out_report->feedback_channels_core = core_ev_sum > 0u ? ((double)core_ch_sum / (double)core_ev_sum) : 0.0;
    out_report->feedback_channels_heavy = heavy_ev_sum > 0u ? ((double)heavy_ch_sum / (double)heavy_ev_sum) : 0.0;
    uint64_t total_ev = core_ev_sum + heavy_ev_sum;
    out_report->feedback_channels_avg = total_ev > 0u ? ((double)(core_ch_sum + heavy_ch_sum) / (double)total_ev) : 0.0;

    double s_lat = out_report->latency_ms <= 33.4 ? 1.0 : clamp01(1.0 - (out_report->latency_ms - 33.4) / 33.6);
    double s_drop = out_report->input_drop_rate <= 0.05 ? 1.0 : clamp01(1.0 - (out_report->input_drop_rate - 0.05) / 0.35);
    double s_nl = out_report->motion_nonlinear ? 1.0 : 0.25;
    double s_juice = 0.5 * clamp01(out_report->feedback_channels_core / 2.5) +
                     0.5 * clamp01(out_report->feedback_channels_heavy / 3.5);
    out_report->m_feel = clamp01(0.25 * s_lat + 0.25 * s_drop + 0.20 * s_nl + 0.30 * s_juice);

    /* --- Pillar 2: Flow & Skill Discrimination (M_flow) --- */
    out_report->w_rand_mean = means[RTS_BOT_NOVICE];
    out_report->w_human_mean = means[RTS_BOT_HUMAN_PROXY];
    out_report->w_human_std = std_h;
    out_report->w_opt_mean = means[RTS_BOT_ORACLE];
    out_report->skill_gap = out_report->w_opt_mean - out_report->w_rand_mean;

    double s_gap = clamp01(out_report->skill_gap / 0.55);
    double s_floor = out_report->w_rand_mean <= 0.35 ? 1.0 : clamp01(1.0 - (out_report->w_rand_mean - 0.35) / 0.35);
    double s_ceil = out_report->w_opt_mean >= 0.70 ? 1.0 : clamp01(out_report->w_opt_mean / 0.70);
    double s_human = range_score(out_report->w_human_mean, 0.40, 0.75, 0.30);
    double s_var = range_score(out_report->w_human_std, 0.10, 0.25, 0.12);
    out_report->m_flow = clamp01(0.30 * s_gap + 0.15 * s_floor + 0.20 * s_ceil + 0.25 * s_human + 0.10 * s_var);

    /* --- Pillar 3: Meaningful Choices & Strategic Entropy (M_choice) --- */
    uint64_t total_acts = 0u;
    for (int a = 0; a < RTS_ACT_COUNT; ++a) total_acts += action_counts[a];
    double h = 0.0;
    double max_p = 0.0;
    for (int a = 0; a < RTS_ACT_COUNT; ++a) {
        double pr = total_acts > 0u ? ((double)action_counts[a] / (double)total_acts) : 0.0;
        out_report->action_probs[a] = pr;
        if (pr > max_p) max_p = pr;
        if (pr > 1e-9) h -= pr * log(pr);
    }
    out_report->h_norm = h / log((double)RTS_ACT_COUNT);
    out_report->max_action_prob = max_p;
    out_report->w_spam_mean = means[RTS_BOT_SPAM];
    out_report->spambot_ratio = out_report->w_spam_mean / fmax(0.05, out_report->w_human_mean);

    double s_ent = clamp01(out_report->h_norm / 0.70);
    double s_maxp = max_p <= 0.65 ? 1.0 : clamp01(1.0 - (max_p - 0.65) / 0.35);
    double s_spam = out_report->spambot_ratio < 0.80 ? 1.0 : clamp01(1.0 - (out_report->spambot_ratio - 0.80) / 0.40);
    out_report->m_choice = clamp01(0.45 * s_ent + 0.20 * s_maxp + 0.35 * s_spam);

    /* --- Pillar 4: Pacing, Tension Waves & Near-Misses (M_pace) --- */
    double t_sum = 0.0;
    for (int i = 0; i < tension_count; ++i) t_sum += tension_samples[i];
    double t_mean = tension_count > 0 ? (t_sum / (double)tension_count) : 0.0;
    double t_var = 0.0;
    for (int i = 0; i < tension_count; ++i) {
        double d = tension_samples[i] - t_mean;
        t_var += d * d;
    }
    double t_std = tension_count > 0 ? sqrt(t_var / (double)tension_count) : 0.0;
    out_report->tension_mean = t_mean;
    out_report->tension_cv = t_mean > 1e-5 ? (t_std / t_mean) : 0.0;
    out_report->max_sustained_high_tension_s = max_sustained_hi_t;
    out_report->near_misses_per_min = human_minutes_sum > 0.0 ? (near_misses_sum / human_minutes_sum) : 0.0;

    double s_cv = range_score(out_report->tension_cv, 0.25, 0.85, 0.20);
    double s_exhaust = max_sustained_hi_t <= 20.0 ? 1.0 : clamp01(1.0 - (max_sustained_hi_t - 20.0) / 20.0);
    double s_nmiss = range_score(out_report->near_misses_per_min, 2.0, 12.0, 4.0);
    out_report->m_pace = clamp01(0.45 * s_cv + 0.15 * s_exhaust + 0.40 * s_nmiss);

    /* --- Pillar 5: Fairness & Perceptual Clarity (M_fair) --- */
    double r_ratio = (double)cfg->player_hurt_radius_m / fmax(0.1, (double)cfg->player_sprite_radius_m);
    out_report->hurtbox_ratio = r_ratio * r_ratio;
    out_report->hitbox_generosity = (double)cfg->attack_hitbox_generosity;
    out_report->telegraph_ms = cfg->telegraph_lead_s * 1000.0;
    out_report->max_shake_vp_pct = (double)cfg->max_shake_vp_frac * 100.0;
    out_report->visual_contrast_ratio = measured_wcag_contrast > 0.0 ? measured_wcag_contrast : 7.2;

    double s_hurt = range_score(out_report->hurtbox_ratio, 0.50, 0.85, 0.20);
    double s_hit = range_score(out_report->hitbox_generosity, 1.05, 1.35, 0.20);
    double s_tele = clamp01(out_report->telegraph_ms / 300.0);
    double s_vis = (out_report->visual_contrast_ratio >= 3.5 && out_report->max_shake_vp_pct <= 4.0) ? 1.0 : 0.5;
    out_report->m_fair = clamp01(0.30 * s_hurt + 0.20 * s_hit + 0.30 * s_tele + 0.20 * s_vis);

    /* --- Pillar 6: Loop Friction & Time-to-Fun (M_loop) --- */
    out_report->ttfa_s = cfg->boot_ttfa_s;
    out_report->retry_s = cfg->instant_in_memory_retry ? (cfg->retry_guard_s + 0.02) : cfg->retry_guard_s;
    out_report->micro_cadence_s = micro_interval_cnt > 0 ? (micro_interval_sum / (double)micro_interval_cnt) : 10.0;
    out_report->meso_cadence_s = meso_interval_cnt > 0 ? (meso_interval_sum / (double)meso_interval_cnt) : 99.0;

    double s_ttfa = out_report->ttfa_s <= 3.0 ? 1.0 : clamp01(1.0 - (out_report->ttfa_s - 3.0) / 3.0);
    double s_retry = out_report->retry_s <= 1.0 ? 1.0 : clamp01(1.0 - (out_report->retry_s - 1.0) / 2.0);
    double s_micro = range_score(out_report->micro_cadence_s, 0.8, 5.0, 4.0);
    double s_meso = range_score(out_report->meso_cadence_s, 18.0, 90.0, 30.0);
    out_report->m_loop = clamp01(0.30 * s_ttfa + 0.35 * s_retry + 0.20 * s_micro + 0.15 * s_meso);

    /* --- Composite Fun Score Formula (Section 4) --- */
    double geo = pow(fmax(1e-6, out_report->m_feel), 0.25) *
                 pow(fmax(1e-6, out_report->m_flow), 0.25) *
                 pow(fmax(1e-6, out_report->m_fair), 0.20);
    double arith = 0.40 + 0.25 * out_report->m_choice + 0.20 * out_report->m_pace + 0.15 * out_report->m_loop;
    out_report->composite_fun_score = clamp01(geo * arith) * 100.0;
    return true;
}

void rts_fun_print_report(const RtsFunAuditReport *rep, FILE *out) {
    if (!rep || !out) return;
    fprintf(out, "=== GAME FUN AUDIT REPORT (%s) ===\n", rep->baseline_mode ? "BASELINE / BEFORE" : "SOLARIS PATCHED / AFTER");
    fprintf(out, "Runs Executed: %d per persona (Novice, Human-Proxy @ 200ms+-35ms, Optimal, SpamBot) | Seed: %llu\n\n",
            rep->episodes_per_persona, (unsigned long long)rep->base_seed);
    fprintf(out, "Pillar Scores (0.00 - 1.00):\n");
    fprintf(out, "- Kinesthetics & Feel (M_feel):   %.2f | Latency: %.1fms, DropRate: %.1f%%, Channels: %.1f (Core %.1f / Heavy %.1f)\n",
            rep->m_feel, rep->latency_ms, rep->input_drop_rate * 100.0, rep->feedback_channels_avg,
            rep->feedback_channels_core, rep->feedback_channels_heavy);
    fprintf(out, "- Flow & Skill Gap (M_flow):      %.2f | Win/Surv: Rand=%.0f%%, Human=%.0f%% (sd=%.2f), Opt=%.0f%% (Gap=%.2f)\n",
            rep->m_flow, rep->w_rand_mean * 100.0, rep->w_human_mean * 100.0, rep->w_human_std,
            rep->w_opt_mean * 100.0, rep->skill_gap);
    fprintf(out, "- Strategic Entropy (M_choice):   %.2f | H_norm=%.2f (maxP=%.2f), SpamBot Ratio=%.2f\n",
            rep->m_choice, rep->h_norm, rep->max_action_prob, rep->spambot_ratio);
    fprintf(out, "- Pacing & Tension (M_pace):      %.2f | Tension CV=%.2f, Near-Misses=%.1f/min\n",
            rep->m_pace, rep->tension_cv, rep->near_misses_per_min);
    fprintf(out, "- Fairness & Clarity (M_fair):    %.2f | Hurtbox Ratio=%.2f, Hitbox=%.2fx, Telegraph=%.0fms, Contrast=%.1f:1\n",
            rep->m_fair, rep->hurtbox_ratio, rep->hitbox_generosity, rep->telegraph_ms, rep->visual_contrast_ratio);
    fprintf(out, "- Loop Friction (M_loop):         %.2f | TTFA=%.2fs, Retry=%.2fs, Micro=%.1fs, Meso=%.1fs\n",
            rep->m_loop, rep->ttfa_s, rep->retry_s, rep->micro_cadence_s, rep->meso_cadence_s);
    fprintf(out, "\nCOMPOSITE FUN SCORE: %.1f / 100\n", rep->composite_fun_score);
}
