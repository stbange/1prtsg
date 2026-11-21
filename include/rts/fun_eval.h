/* SPDX-License-Identifier: MIT */
#ifndef RTS_FUN_EVAL_H
#define RTS_FUN_EVAL_H

#include "rts/tactical_sim.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef enum RtsBotPersona {
    RTS_BOT_NOVICE      = 0,
    RTS_BOT_HUMAN_PROXY = 1,
    RTS_BOT_ORACLE      = 2,
    RTS_BOT_SPAM        = 3,
    RTS_BOT_COUNT       = 4
} RtsBotPersona;

typedef struct RtsFunAuditReport {
    int episodes_per_persona;
    uint64_t base_seed;
    bool baseline_mode;

    /* Pillar 1: Kinesthetics & Feel */
    double m_feel;
    double latency_ms;
    double input_drop_rate;
    bool motion_nonlinear;
    double feedback_channels_core;
    double feedback_channels_heavy;
    double feedback_channels_avg;

    /* Pillar 2: Flow & Skill Discrimination */
    double m_flow;
    double w_rand_mean;
    double w_human_mean;
    double w_human_std;
    double w_opt_mean;
    double skill_gap;

    /* Pillar 3: Meaningful Choices & Strategic Entropy */
    double m_choice;
    double h_norm;
    double max_action_prob;
    double action_probs[RTS_ACT_COUNT];
    double w_spam_mean;
    double spambot_ratio;

    /* Pillar 4: Pacing, Tension Waves & Near-Misses */
    double m_pace;
    double tension_mean;
    double tension_cv;
    double max_sustained_high_tension_s;
    double near_misses_per_min;

    /* Pillar 5: Fairness & Perceptual Clarity */
    double m_fair;
    double hurtbox_ratio;
    double hitbox_generosity;
    double telegraph_ms;
    double max_shake_vp_pct;
    double visual_contrast_ratio;

    /* Pillar 6: Loop Friction & Time-to-Fun */
    double m_loop;
    double ttfa_s;
    double retry_s;
    double micro_cadence_s;
    double meso_cadence_s;

    /* Composite Fun Score [0, 100] */
    double composite_fun_score;
} RtsFunAuditReport;

/* Run N episodes per persona (Novice, Human-Proxy @ 200ms+-35ms, Oracle, SpamBot).
 * If telemetry_jsonl_path is non-NULL, writes 60Hz structured JSONL telemetry for the first Human-Proxy run. */
bool rts_fun_run_audit(const RtsFeelConfig *cfg,
                       uint64_t base_seed,
                       int episodes_per_persona,
                       const char *telemetry_jsonl_path,
                       double measured_wcag_contrast,
                       RtsFunAuditReport *out_report);

/* Print the exact Section 7 GAME FUN AUDIT REPORT format */
void rts_fun_print_report(const RtsFunAuditReport *rep, FILE *out);

/* Utility: run one bot step against live sim (also useful for capturing peak_tension / game_over visual frames!) */
void rts_fun_bot_decide(RtsBotPersona persona,
                        const RtsTacticalSim *sim,
                        uint64_t *bot_rng,
                        RtsTacticalInput *out_in);

#endif /* RTS_FUN_EVAL_H */
