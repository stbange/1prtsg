/* SPDX-License-Identifier: MIT */
#include "rts/economy.h"
#include "rts/gfx.h"
#include "rts/ui_menus.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void test_solaris_menu_navigation_defaults_and_clicks(void) {
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsEconomySystem *eco = (RtsEconomySystem *)calloc(1, sizeof(RtsEconomySystem));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(eco);
    rts_world_init(w, rts_planet_earthlike(42u), 77u);
    rts_economy_init(eco);

    RtsConstructBlueprint bp;
    memset(&bp, 0, sizeof(bp));
    snprintf(bp.id, sizeof(bp.id), "bldg_command_citadel");
    snprintf(bp.name, sizeof(bp.name), "Solar Command Citadel");
    bp.construct_class = RTS_CLASS_BUILDING;
    bp.max_hull = 4000.0f; bp.active = true;
    rts_world_register_blueprint(w, &bp);
    rts_world_spawn_from_blueprint(w, "bldg_command_citadel", "Citadel Alpha", 1u,
                                   w->planet_frame_id, rts_v3d(0.0, w->planet.radius_m, 0.0), true);

    RtsMenuState ms;
    rts_menu_init(&ms);
    /* Verify User-Approved Defaults */
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MAIN, ms.screen);
    TEST_ASSERT_TRUE(ms.skirmish_ironman);
    TEST_ASSERT_FALSE(ms.skirmish_ai_aggressive);
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 6371000.0, rts_planet_preset_radius_m(ms.planet_radius_preset));
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 1200000.0, rts_planet_preset_radius_m(0));
    TEST_ASSERT_DOUBLE_WITHIN(1.0, 12000000.0, rts_planet_preset_radius_m(2));
    TEST_ASSERT_EQUAL_STRING("orbital_drop_defense", rts_scenario_id(0));
    TEST_ASSERT_EQUAL_STRING("orbital_drop_defense", rts_scenario_id(99));
    TEST_ASSERT_NOT_NULL(rts_scenario_title(2));
    TEST_ASSERT_NOT_NULL(rts_scenario_brief(3));

    /* Exercise every screen title, item count, and item label */
    char lbl[128];
    RtsMenuScreen screens[] = {
        RTS_MENU_MAIN, RTS_MENU_SKIRMISH_SETUP, RTS_MENU_SCENARIOS,
        RTS_MENU_SANDBOX_PICKER, RTS_MENU_SAVE_LOAD, RTS_MENU_MODS,
        RTS_MENU_PAUSE, RTS_MENU_MATCH_END, RTS_MENU_NONE
    };
    for (size_t s = 0; s < sizeof(screens) / sizeof(screens[0]); ++s) {
        ms.screen = screens[s];
        TEST_ASSERT_NOT_NULL(rts_menu_screen_title(ms.screen));
        int cnt = rts_menu_item_count(&ms);
        for (int i = 0; i < cnt; ++i) {
            rts_menu_item_label(&ms, w, i, lbl, sizeof(lbl));
            TEST_ASSERT_TRUE(lbl[0] != '\0');
        }
    }

    /* Main Menu -> Skirmish Setup -> toggle options -> Launch Skirmish */
    rts_menu_init(&ms);
    rts_menu_move_selection(&ms, -1);
    TEST_ASSERT_EQUAL_INT(5, ms.selected_row);
    rts_menu_move_selection(&ms, +1);
    TEST_ASSERT_EQUAL_INT(0, ms.selected_row);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_SKIRMISH_SETUP, ms.screen);
    ms.selected_row = 0; rts_menu_activate(&ms);
    ms.selected_row = 1; rts_menu_activate(&ms);
    ms.selected_row = 2; rts_menu_activate(&ms);
    TEST_ASSERT_FALSE(ms.skirmish_ironman);
    TEST_ASSERT_TRUE(ms.skirmish_ai_aggressive);
    ms.selected_row = 3;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_START_SKIRMISH, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_NONE, ms.screen);

    /* Main Menu -> Scenarios -> Click Row 2 (Escape Flagship 60s) */
    rts_menu_init(&ms);
    ms.selected_row = 3; rts_menu_activate(&ms);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_SCENARIOS, ms.screen);
    RtsMenuAction ca = rts_menu_click(&ms, 640.0f, 360.0f - 210.0f + 86.0f + 2.0f * 48.0f + 16.0f, 1280.0f, 720.0f);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_LAUNCH_SCENARIO, ca);
    TEST_ASSERT_EQUAL_INT(2, ms.scenario_idx);

    /* Free-Explore Sandbox Picker -> cycle & launch */
    rts_menu_init(&ms);
    ms.selected_row = 1; rts_menu_activate(&ms);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_SANDBOX_PICKER, ms.screen);
    ms.selected_row = 1; rts_menu_activate(&ms);
    TEST_ASSERT_EQUAL_INT(1, ms.sandbox_bp_idx);
    ms.selected_row = 2;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_START_SANDBOX, rts_menu_activate(&ms));

    /* Tactical Defense, Mods, Pause, Match End & Back navigation */
    rts_menu_init(&ms);
    ms.selected_row = 2;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_START_TACTICAL, rts_menu_activate(&ms));

    rts_menu_init(&ms);
    ms.selected_row = 5; rts_menu_activate(&ms);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_TOGGLE_MOD, rts_menu_activate(&ms));
    ms.selected_row = 1;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_HOT_RELOAD, rts_menu_activate(&ms));
    rts_menu_back(&ms);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MAIN, ms.screen);

    ms.screen = RTS_MENU_PAUSE; ms.selected_row = 0;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_RESUME, rts_menu_activate(&ms));
    ms.screen = RTS_MENU_PAUSE; rts_menu_back(&ms);
    TEST_ASSERT_EQUAL_INT(RTS_MENU_NONE, ms.screen);

    ms.screen = RTS_MENU_MATCH_END; ms.selected_row = 0;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_START_SKIRMISH, rts_menu_activate(&ms));

    /* 5-Slot Persistent Save/Load Round-Trip on Disk (portable current dir for Linux/macOS/Windows) */
    rts_menu_init(&ms);
    w->sim_time_s = 123.5;
    TEST_ASSERT_TRUE(rts_menu_save_slot(&ms, 0, ".", w, eco));
    TEST_ASSERT_TRUE(ms.slots[0].occupied);
    rts_menu_refresh_slots(&ms, ".");
    w->sim_time_s = 0.0;
    TEST_ASSERT_TRUE(rts_menu_load_slot(&ms, 0, ".", w, eco));
    TEST_ASSERT_DOUBLE_WITHIN(0.1, 123.5, w->sim_time_s);
    TEST_ASSERT_FALSE(rts_menu_load_slot(&ms, 4, "./nonexistent_slot_dir", w, eco));
    remove("./1prtsg_slot_1.rts");

    /* Null-backend renderer pass across Main, Scenarios, Save/Load, and Match End Defeat */
    RtsVfs vfs;
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    TEST_ASSERT_NOT_NULL(rr);
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1280, 720));

    ms.screen = RTS_MENU_MAIN;
    rts_renderer_render_menu_screen(rr, w, &ms, 1.0f);
    ms.screen = RTS_MENU_SCENARIOS;
    rts_renderer_render_menu_screen(rr, w, &ms, 2.0f);
    ms.screen = RTS_MENU_MATCH_END;
    ms.victory = false;
    rts_renderer_render_menu_screen(rr, w, &ms, 3.0f);
    TEST_ASSERT_TRUE(rr->gfx.get_stats(rr->gfx.state).draw_calls > 15u);

    rts_renderer_shutdown(rr);
    free(rr);
    free(eco);
    free(w);
}

void register_skirmish_menus_tests(void) {
    RUN_TEST(test_solaris_menu_navigation_defaults_and_clicks);
}
