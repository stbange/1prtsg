/* SPDX-License-Identifier: MIT */
#include "rts/audio.h"
#include "rts/gfx.h"
#include "rts/lua_vm.h"
#include "rts/mod_manager.h"
#include "rts/ui_menus.h"
#include "rts/vfs.h"
#include "rts/world.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

static void test_full_27_wav_audio_suite_and_adaptive_music(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    RtsPlanetConfig planet = rts_planet_earthlike(42u);

    RtsAudioEngine ae;
    TEST_ASSERT_TRUE(rts_audio_init(&ae, rts_audio_null_create(), &vfs, 22050));
    rts_audio_set_listener(&ae, rts_v3d(0.0, planet.radius_m, 0.0), rts_v3d(1.0, 0.0, 0.0), 0.0, 0.0f);

    /* Every single external CC BY 4.0 WAV in the 27-asset catalog must exist and decode cleanly */
    size_t cnt = rts_audio_catalog_count();
    TEST_ASSERT_EQUAL_UINT32(27u, (uint32_t)cnt);
    TEST_ASSERT_NULL(rts_audio_catalog_path(cnt));
    for (size_t i = 0; i < cnt; ++i) {
        const char *p = rts_audio_catalog_path(i);
        TEST_ASSERT_NOT_NULL(p);
        RtsAudioVoice v = rts_audio_play_3d(&ae, &planet, p, rts_v3d(5.0, planet.radius_m, 0.0),
                                            RTS_AUD_RADIO_COMMS, 0.7f, 200.0f);
        TEST_ASSERT_TRUE(v.audible);
    }
    RtsAudioStats st = ae.backend.get_stats(ae.backend.state);
    TEST_ASSERT_EQUAL_UINT32(27u, st.wavs_decoded);

    /* Adaptive music transitions + cooldown gating */
    TEST_ASSERT_TRUE(rts_audio_update_music(&ae, RTS_MUSIC_SOLARIS_ORBIT, 0.016));
    TEST_ASSERT_FALSE(rts_audio_update_music(&ae, RTS_MUSIC_SOLARIS_ORBIT, 0.5));
    TEST_ASSERT_TRUE(rts_audio_update_music(&ae, RTS_MUSIC_SURFACE_COMMAND, 0.016));
    TEST_ASSERT_TRUE(rts_audio_update_music(&ae, RTS_MUSIC_RED_ALERT_BATTLE, 0.016));
    TEST_ASSERT_TRUE(rts_audio_update_music(&ae, RTS_MUSIC_RED_ALERT_BATTLE, 3.5));
    TEST_ASSERT_FALSE(rts_audio_update_music(NULL, RTS_MUSIC_SOLARIS_ORBIT, 0.1));

    /* All semantic SFX event names + vacuum silencing */
    static const char *const events[] = {
        "plasma", "beam", "torpedo", "flak", "orbital_ion", "emp", "shield",
        "klaxon", "docking", "footstep", "airlock", "turbolift",
        "decompression", "escape_pod", "explosion", "victory", "unknown_fallback"
    };
    for (size_t i = 0; i < sizeof(events) / sizeof(events[0]); ++i) {
        RtsAudioVoice v = rts_audio_play_event(&ae, &planet, events[i],
                                               rts_v3d(8.0, planet.radius_m, 0.0),
                                               RTS_AUD_INTERIOR_SFX);
        TEST_ASSERT_TRUE(v.audible);
    }
    rts_audio_set_listener(&ae, rts_v3d(0.0, planet.radius_m + 300000.0, 0.0), rts_v3d(1.0, 0.0, 0.0), 300000.0, 0.0f);
    RtsAudioVoice vac = rts_audio_play_event(&ae, &planet, "explosion",
                                             rts_v3d(5.0, planet.radius_m + 300000.0, 0.0),
                                             RTS_AUD_EXTERIOR_AIRBORNE);
    TEST_ASSERT_FALSE(vac.audible);

    rts_audio_shutdown(&ae);
}

static void test_mod_manager_discovery_toggle_and_live_hot_reload(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));

    RtsModCatalog cat;
    size_t found = rts_mod_manager_scan(&cat, &vfs);
    TEST_ASSERT_TRUE(found >= 2u);
    TEST_ASSERT_EQUAL_STRING("example_mod", cat.entries[0].id);
    TEST_ASSERT_EQUAL_STRING("solaris_gold_fleet", cat.entries[1].id);
    TEST_ASSERT_FALSE(cat.entries[0].enabled);

    /* Mount via toggle, check idempotent re-mount, and verify menu integration */
    TEST_ASSERT_TRUE(rts_mod_manager_toggle(&cat, &vfs, 0u));
    TEST_ASSERT_TRUE(cat.entries[0].enabled);
    TEST_ASSERT_TRUE(rts_vfs_mount_mod(&vfs, "example_mod")); /* Idempotent */
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)vfs.mod_count);

    /* Live hot-reload of Lua + Renderer textures */
    RtsWorld *w = (RtsWorld *)calloc(1, sizeof(RtsWorld));
    RtsRetroRenderer *rr = (RtsRetroRenderer *)calloc(1, sizeof(RtsRetroRenderer));
    RtsLuaVm *lua = (RtsLuaVm *)calloc(1, sizeof(RtsLuaVm));
    TEST_ASSERT_NOT_NULL(w);
    TEST_ASSERT_NOT_NULL(rr);
    TEST_ASSERT_NOT_NULL(lua);
    rts_world_init(w, rts_planet_earthlike(42u), 99u);
    TEST_ASSERT_TRUE(rts_renderer_init(rr, rts_gfx_null_create(), &vfs, 1280, 720));
    TEST_ASSERT_TRUE(rts_lua_init(lua, &vfs, w, NULL, NULL, rr));
    TEST_ASSERT_TRUE(rts_mod_manager_hot_reload(&vfs, lua, rr));

    /* Verify RTS_MENU_MODS dynamic labels, toggle, hot-reload, and back navigation */
    RtsMenuState ms;
    rts_menu_init(&ms);
    rts_menu_sync_mods(&ms, &vfs);
    TEST_ASSERT_TRUE(ms.mod_catalog.count >= 2u);
    TEST_ASSERT_EQUAL_STRING("example_mod", ms.active_mod_id);
    ms.screen = RTS_MENU_MODS;
    ms.return_screen = RTS_MENU_MAIN;
    int mcnt = rts_menu_item_count(&ms);
    TEST_ASSERT_EQUAL_INT((int)ms.mod_catalog.count + 2, mcnt);
    char lbl[128];
    for (int i = 0; i < mcnt; ++i) {
        rts_menu_item_label(&ms, w, i, lbl, sizeof(lbl));
        TEST_ASSERT_TRUE(lbl[0] != '\0');
    }
    ms.selected_row = 0;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_TOGGLE_MOD, rts_menu_activate(&ms));
    ms.selected_row = (int)ms.mod_catalog.count;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_HOT_RELOAD, rts_menu_activate(&ms));
    ms.selected_row = (int)ms.mod_catalog.count + 1;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MAIN, ms.screen);

    /* Extra pause/save/match-end menu branch coverage */
    ms.screen = RTS_MENU_PAUSE;
    ms.selected_row = 1; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_SAVE_SLOT, rts_menu_activate(&ms));
    ms.selected_row = 2; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_SAVE_LOAD, ms.screen);
    ms.selected_row = 0; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_SAVE_SLOT, rts_menu_activate(&ms));
    ms.selected_row = RTS_SAVE_SLOT_COUNT; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_PAUSE, ms.screen);
    ms.selected_row = 3; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MODS, ms.screen);
    ms.screen = RTS_MENU_PAUSE; ms.selected_row = 4;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MAIN, ms.screen);
    ms.screen = RTS_MENU_MATCH_END; ms.selected_row = 1;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_MAIN, ms.screen);
    ms.screen = RTS_MENU_MAIN; ms.selected_row = 4;
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_activate(&ms));
    ms.selected_row = 0; TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_LOAD_SLOT, rts_menu_activate(&ms));
    TEST_ASSERT_EQUAL_INT(RTS_MENU_ACT_NONE, rts_menu_click(&ms, 5.0f, 5.0f, 1280.0f, 720.0f));

    /* Unmount & clear mods cleanly */
    TEST_ASSERT_TRUE(rts_mod_manager_toggle(&cat, &vfs, 0u));
    TEST_ASSERT_FALSE(cat.entries[0].enabled);
    TEST_ASSERT_FALSE(rts_vfs_unmount_mod(&vfs, "nonexistent_mod"));
    TEST_ASSERT_TRUE(rts_mod_manager_toggle(&cat, &vfs, 1u));
    rts_vfs_clear_mods(&vfs);
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)vfs.mod_count);
    TEST_ASSERT_FALSE(rts_mod_manager_toggle(&cat, &vfs, 99u));
    TEST_ASSERT_FALSE(rts_mod_manager_hot_reload(NULL, NULL, NULL));

    rts_lua_shutdown(lua);
    rts_renderer_shutdown(rr);
    free(lua);
    free(rr);
    free(w);
}

void register_audio_mods_tests(void) {
    RUN_TEST(test_full_27_wav_audio_suite_and_adaptive_music);
    RUN_TEST(test_mod_manager_discovery_toggle_and_live_hot_reload);
}
