/* SPDX-License-Identifier: MIT */
#include "rts/audio.h"
#include "rts/fun_eval.h"
#include "rts/gfx.h"
#include "rts/tactical_sim.h"
#include "rts/vfs.h"
#include "unity.h"
#include <stdio.h>
#include <string.h>

static void test_tactical_sim_kinesthetics_synergy_retry_and_renderer(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    RtsAudioEngine audio;
    TEST_ASSERT_TRUE(rts_audio_init(&audio, rts_audio_null_create(), &vfs, 22050));

    RtsFeelConfig cfg = rts_feel_config_solaris();
    RtsTacticalSim sim;
    rts_tactical_init(&sim, &cfg, 42u, &audio);
    TEST_ASSERT_TRUE(sim.active_run);
    TEST_ASSERT_TRUE(rts_tactical_active_enemy_count(&sim) >= 2u);

    /* Non-linear acceleration + jump apex hang-time + input buffer */
    RtsTacticalInput in;
    memset(&in, 0, sizeof(in));
    in.move_z = 1.0f;
    in.jump_pressed = true;
    in.action_pressed[RTS_ACT_PLASMA_LANCE] = true;
    rts_tactical_step(&sim, &in, 1.0 / 60.0);
    TEST_ASSERT_FALSE(sim.player_grounded);
    TEST_ASSERT_TRUE(sim.player_vel.z < 0.0f && sim.player_vel.z > -10.5f); /* exponential ramp, not instant boxcar */

    /* Press EMP, Dash, Fleet Strike, Field Repair across ticks with audio & particle juice */
    in.jump_pressed = false;
    in.action_pressed[RTS_ACT_PLASMA_LANCE] = false;
    uint64_t bot_rng = 99u;
    for (int t = 0; t < 480 && sim.active_run; ++t) {
        rts_fun_bot_decide(RTS_BOT_ORACLE, &sim, &bot_rng, &in);
        rts_tactical_step(&sim, &in, 1.0 / 60.0);
    }
    TEST_ASSERT_TRUE(sim.score > 0.0f);
    TEST_ASSERT_TRUE(rts_tactical_nearest_threat_pw(&sim) >= 0.0f);

    /* Render tactical live world on null backend (active combat + game over modal) */
    RtsWorld world;
    rts_world_init(&world, rts_planet_earthlike(42u), 42u);
    rts_tactical_sync_to_world(&sim, &world);
    RtsInputRouter router;
    rts_input_init(&router, 1280.0f, 720.0f, false);
    RtsRetroRenderer rr;
    TEST_ASSERT_TRUE(rts_renderer_init(&rr, rts_gfx_null_create(), &vfs, 1280, 720));
    rts_renderer_render_tactical(&rr, &sim, &world, &router, (float)sim.sim_time_s);

    /* Trigger Ironman Defeat, verify 250ms retry guard, then verify instant in-memory retry */
    sim.health = 1.0f;
    sim.exo_shield = 0.0f;
    sim.iframe_timer_s = 0.0f;
    sim.dash_timer_s = 0.0f;
    /* Place an enemy projectile right on player to trigger authentic Ironman defeat */
    sim.projectiles[0].active = true;
    sim.projectiles[0].from_player = false;
    sim.projectiles[0].pos = sim.player_pos;
    sim.projectiles[0].vel = (RtsVec3f){0.0f, 0.0f, 1.0f};
    sim.projectiles[0].damage = 50.0f;
    sim.projectiles[0].radius_m = 0.6f;
    sim.projectiles[0].life_s = 2.0f;
    memset(&in, 0, sizeof(in));
    rts_tactical_step(&sim, &in, 1.0 / 60.0);
    TEST_ASSERT_TRUE(sim.game_over);
    rts_tactical_sync_to_world(&sim, &world);
    TEST_ASSERT_EQUAL_INT(RTS_MATCH_DEFEAT_IRONMAN, world.match_state);
    rts_renderer_render_tactical(&rr, &sim, &world, &router, (float)sim.sim_time_s);

    /* Accidental-input guard rejects retry < 250ms after death */
    TEST_ASSERT_FALSE(rts_tactical_reset_fast(&sim, 99u));
    /* Advance 300ms then press retry */
    in.retry_pressed = true;
    for (int i = 0; i < 20 && sim.game_over; ++i) {
        rts_tactical_step(&sim, &in, 1.0 / 60.0);
    }
    TEST_ASSERT_TRUE(sim.active_run && !sim.game_over);

    rts_renderer_shutdown(&rr);
    rts_audio_shutdown(&audio);
}

static void test_fun_eval_audit_baseline_vs_solaris(void) {
    RtsFeelConfig base = rts_feel_config_baseline();
    RtsFeelConfig solaris = rts_feel_config_solaris();

    RtsFunAuditReport rep_base, rep_sol;
    TEST_ASSERT_TRUE(rts_fun_run_audit(&base, 42u, 6, NULL, 7.2, &rep_base));
    TEST_ASSERT_TRUE(rts_fun_run_audit(&solaris, 42u, 10, "rts_unit_fun_telemetry.jsonl", 7.2, &rep_sol));

    /* Verify JSONL telemetry file was emitted cleanly */
    FILE *fp = fopen("rts_unit_fun_telemetry.jsonl", "r");
    TEST_ASSERT_NOT_NULL(fp);
    char line[1024];
    TEST_ASSERT_NOT_NULL(fgets(line, sizeof(line), fp));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"lifecycle\""));
    TEST_ASSERT_NOT_NULL(strstr(line, "\"juice\""));
    fclose(fp);
    remove("rts_unit_fun_telemetry.jsonl");

    /* Report printing smoke test */
    FILE *sink = fopen("/dev/null", "w");
    if (sink) {
        rts_fun_print_report(&rep_base, sink);
        rts_fun_print_report(&rep_sol, sink);
        fclose(sink);
    }

    /* Patched Solaris must substantially outperform unpatched Baseline across core pillars */
    TEST_ASSERT_TRUE(rep_sol.composite_fun_score >= 88.0);
    TEST_ASSERT_TRUE(rep_sol.composite_fun_score > rep_base.composite_fun_score + 30.0);
    TEST_ASSERT_TRUE(rep_sol.m_feel >= 0.90);
    TEST_ASSERT_TRUE(rep_sol.m_choice >= 0.85);
    TEST_ASSERT_TRUE(rep_sol.m_fair >= 0.95);
    TEST_ASSERT_TRUE(rep_sol.m_loop >= 0.90);
    TEST_ASSERT_TRUE(rep_sol.input_drop_rate < 0.05);
    TEST_ASSERT_TRUE(rep_sol.skill_gap >= 0.55);
    TEST_ASSERT_TRUE(rep_sol.spambot_ratio < 0.80);

    /* Negative input guards */
    TEST_ASSERT_FALSE(rts_fun_run_audit(NULL, 42u, 5, NULL, 7.0, &rep_sol));
    TEST_ASSERT_FALSE(rts_fun_run_audit(&solaris, 42u, 0, NULL, 7.0, &rep_sol));
}

void register_fun_eval_tests(void) {
    RUN_TEST(test_tactical_sim_kinesthetics_synergy_retry_and_renderer);
    RUN_TEST(test_fun_eval_audit_baseline_vs_solaris);
}
