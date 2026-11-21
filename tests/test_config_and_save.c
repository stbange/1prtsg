/* SPDX-License-Identifier: MIT */
#include "rts/config.h"
#include "rts/save_load.h"
#include "unity.h"
#include <string.h>

typedef struct MockClockFs {
    uint64_t mock_mtime;
} MockClockFs;

static bool mock_exists(void *u, const char *p) { (void)u; return p != NULL; }
static bool mock_read(void *u, const char *p, uint8_t **b, size_t *s) { (void)u; (void)p; (void)b; (void)s; return false; }
static uint64_t mock_mtime(void *u, const char *p) { (void)p; return ((MockClockFs *)u)->mock_mtime; }

static void test_external_config_and_vfs_hot_reload(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));

    RtsEngineConfig cfg;
    TEST_ASSERT_TRUE(rts_config_load_vfs(&vfs, "engine.cfg", &cfg));
    TEST_ASSERT_EQUAL_INT(1280, cfg.window_width);
    TEST_ASSERT_EQUAL_INT(720, cfg.window_height);
    TEST_ASSERT_TRUE(cfg.retro_nearest_textures);
    TEST_ASSERT_TRUE(cfg.hardcore_ironman);
    TEST_ASSERT_DOUBLE_WITHIN(1e-3, 45.0, cfg.crew_sprite_lod_distance_m);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 6371000.0, cfg.planet_radius_m);

    /* Missing config falls back cleanly to defaults */
    TEST_ASSERT_FALSE(rts_config_load_vfs(&vfs, "missing.cfg", &cfg));
    TEST_ASSERT_EQUAL_INT(1280, cfg.window_width);
    TEST_ASSERT_FALSE(rts_config_parse_text(NULL, &cfg));

    /* Custom config string with enabled_mods, comments & non-default flags */
    const char *custom =
        "# Modded test config\n"
        "window_width = 1920\n"
        "window_height = 1080\n"
        "retro_nearest_textures = false\n"
        "hardcore_ironman = 0\n"
        "crew_sprite_lod_distance_m = 60.0\n"
        "planet_radius_m = 7000000.0\n"
        "enabled_mods = example_mod, non_existent_mod\n";
    TEST_ASSERT_TRUE(rts_config_parse_text(custom, &cfg));
    TEST_ASSERT_EQUAL_INT(1920, cfg.window_width);
    TEST_ASSERT_FALSE(cfg.hardcore_ironman);
    TEST_ASSERT_EQUAL_UINT(2u, cfg.enabled_mod_count);

    RtsWorld w;
    rts_world_init(&w, rts_planet_earthlike(42u), 10u);
    rts_config_apply(&cfg, &vfs, &w);
    TEST_ASSERT_FALSE(w.hardcore_ironman);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 7000000.0, w.planet.radius_m);
    TEST_ASSERT_EQUAL_UINT(1u, vfs.mod_count); /* example_mod mounted, non_existent skipped cleanly */

    /* Live VFS Hot-Reload Watcher with deterministic mtime progression */
    MockClockFs clk = {1000u};
    RtsFsBackend mfs = {&clk, mock_exists, mock_read, mock_mtime};
    RtsVfs mvfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&mvfs, "/virtual", &mfs));

    RtsHotReloadWatch watch;
    rts_vfs_watch_init(&watch);
    TEST_ASSERT_TRUE(rts_vfs_watch_add(&mvfs, &watch, "scripts", "main.lua"));
    TEST_ASSERT_FALSE(rts_vfs_watch_add(&vfs, &watch, "scripts", "does_not_exist.lua"));

    size_t changed_idx[4];
    TEST_ASSERT_EQUAL_UINT(0u, rts_vfs_check_hot_reload(&mvfs, &watch, changed_idx, 4));
    clk.mock_mtime = 2000u; /* File edited on disk! */
    TEST_ASSERT_EQUAL_UINT(1u, rts_vfs_check_hot_reload(&mvfs, &watch, changed_idx, 4));
    TEST_ASSERT_EQUAL_UINT(0u, changed_idx[0]);
    /* Next poll with unchanged mtime reports 0 */
    TEST_ASSERT_EQUAL_UINT(0u, rts_vfs_check_hot_reload(&mvfs, &watch, changed_idx, 4));
}

static void add_save_bp(RtsWorld *w, const char *id, float spd) {
    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    strncpy(bp.id, id, sizeof(bp.id) - 1);
    strncpy(bp.name, id, sizeof(bp.name) - 1);
    bp.construct_class = RTS_CLASS_GROUND_VEHICLE;
    bp.max_hull = 800;
    bp.max_shield = 400;
    bp.max_speed_mps = spd;
    bp.crew_capacity = 4;
    rts_world_register_blueprint(w, &bp);
}

static void test_deterministic_save_load_round_trip(void) {
    RtsWorld w1, w2;
    rts_world_init(&w1, rts_planet_earthlike(42u), 999u);
    rts_world_init(&w2, rts_planet_earthlike(42u), 1u);
    add_save_bp(&w1, "veh_plasma_tank", 25.0f);
    add_save_bp(&w2, "veh_plasma_tank", 25.0f);

    RtsEconomySystem e1, e2;
    rts_economy_init(&e1);
    rts_economy_init(&e2);
    rts_economy_add_resources(&e1, 1, 777.5, 333.25, 120.0, 20u);

    uint32_t t1 = rts_world_spawn_from_blueprint(&w1, "veh_plasma_tank", "Tank-1", 1, w1.planet_frame_id, rts_v3d(10, 6371000, 0), false);
    rts_world_issue_order(&w1, t1, RTS_ORDER_MOVE, rts_v3d(250, 6371000, 40), 0, 0);
    rts_world_apply_damage(&w1, t1, 150.0f, RTS_SUBSYS_ENGINES);
    w1.commander.mag_boots = true;
    w1.commander.wrist_pad_open = true;
    for (int i = 0; i < 10; ++i) rts_world_step(&w1, 0.1);

    /* Serialize to file and deserialize into fresh world + economy */
    const char *save_path = "1prtsg_test_snapshot.sav";
    TEST_ASSERT_TRUE(rts_save_to_file(save_path, &w1, &e1));
    TEST_ASSERT_TRUE(rts_load_from_file(save_path, &w2, &e2));
    remove(save_path);

    /* Reject malformed snapshots & tiny buffers */
    char tiny[16];
    TEST_ASSERT_EQUAL_UINT(0u, rts_save_serialize(&w1, &e1, tiny, sizeof(tiny)));
    TEST_ASSERT_FALSE(rts_save_deserialize("BAD_HEADER\n", &w2, &e2));
    TEST_ASSERT_FALSE(rts_load_from_file("nonexistent_1prtsg_xyz.sav", &w2, &e2));

    /* Step BOTH original and restored worlds 20 ticks -> MUST match deterministically! */
    for (int i = 0; i < 20; ++i) {
        rts_world_step(&w1, 0.1);
        rts_world_step(&w2, 0.1);
    }
    const RtsEntity *a = rts_world_get_entity_const(&w1, t1);
    const RtsEntity *b = rts_world_get_entity_const(&w2, t1);
    TEST_ASSERT_NOT_NULL(a);
    TEST_ASSERT_NOT_NULL(b);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, a->pos.x, b->pos.x);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, a->pos.y, b->pos.y);
    TEST_ASSERT_DOUBLE_WITHIN(1e-9, a->pos.z, b->pos.z);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, a->hull, b->hull);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, a->shield, b->shield);
    TEST_ASSERT_EQUAL_UINT64(w1.tick_count, w2.tick_count);
    TEST_ASSERT_EQUAL_UINT64(w1.rng_state, w2.rng_state);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, e1.ledgers[1].ore, e2.ledgers[1].ore);
    TEST_ASSERT_TRUE(w2.commander.mag_boots && w2.commander.wrist_pad_open);
}

void register_config_and_save_tests(void) {
    RUN_TEST(test_external_config_and_vfs_hot_reload);
    RUN_TEST(test_deterministic_save_load_round_trip);
}
