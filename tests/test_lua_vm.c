/* SPDX-License-Identifier: MIT */
#include "rts/lua_vm.h"
#include "unity.h"

static void test_lua_vm_bindings_sandbox_and_coverage(void) {
    RtsVfs vfs;
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, RTS_TEST_DATA_DIR, NULL));
    RtsWorld world;
    rts_world_init(&world, rts_planet_earthlike(42u), 777u);
    RtsInputRouter input;
    rts_input_init(&input, 1280.0f, 720.0f, false);
    RtsVoxelChunk voxel;
    rts_voxel_init(&voxel, 1.0f);

    RtsAudioEngine audio;
    rts_audio_init(&audio, rts_audio_null_create(), &vfs, 22050);
    RtsEconomySystem eco;
    rts_economy_init(&eco);
    RtsCommanderController ctrl;
    rts_controller_init(&ctrl);
    RtsBoardingSystem boarding;
    rts_boarding_init(&boarding);

    RtsLuaVm vm;
    TEST_ASSERT_FALSE(rts_lua_init(&vm, NULL, &world, &input, &voxel, NULL));
    TEST_ASSERT_TRUE(rts_lua_init(&vm, &vfs, &world, &input, &voxel, NULL));
    rts_lua_attach_phase2(&vm, &audio, &eco, &ctrl);
    rts_lua_attach_phase3(&vm, &boarding);
    rts_lua_enable_coverage(&vm, true);

    /* Run full Lua unit test suite inside C unit test to exercise every binding */
    TEST_ASSERT_TRUE_MESSAGE(rts_lua_exec_vfs_script(&vm, "tests/unit/run_all.lua"), vm.last_error);
    double cov = rts_lua_report_coverage(&vm, 95.0, false);
    TEST_ASSERT_TRUE(cov >= 95.0);

    /* Exercise remaining engine.vfs / orbit / audio / economy / control / boarding / error branches */
    TEST_ASSERT_TRUE_MESSAGE(rts_lua_exec_string(&vm,
        "assert(engine.vfs.exists('scripts', 'main.lua'))\n"
        "assert(engine.vfs.read_text('scripts', 'main.lua') ~= nil)\n"
        "assert(engine.vfs.read_text('scripts', 'nope.lua') == nil)\n"
        "assert(engine.orbit.circular_speed(400000) > 7000)\n"
        "assert(engine.world.get_entity(99999) == nil)\n"
        "assert(not engine.world.set_orbital(99999, 400000))\n"
        "assert(not engine.world.commander_board(99999))\n"
        "local m, x, y, z = engine.input.simulate_touch()\n"
        "assert(m == 2 and x > 0)\n"
        "local xm, hit, sel = engine.input.simulate_webxr()\n"
        "assert(xm == 3 and hit and sel)\n"
        "assert(engine.voxel.test_mesh() == 60)\n"
        "assert(engine.vfs.mount_mod('example_mod'))\n"
        "engine.audio.set_listener(0, 0, 0, 400000, 0.0)\n"
        "local vg, va = engine.audio.play_3d('audio/plasma_bolt.wav', 10, 0, 0, 0, 1.0, 200)\n"
        "assert(not va and vg == 0)\n"
        "assert(engine.economy.ledger(99) == nil)\n"
        "assert(engine.economy.job_spawned(9999) == 0)\n"
        "assert(engine.control.engage_pilot(ACTIVE_MISSION.dropship))\n"
        "engine.control.release_pilot()\n"
        "assert(engine.boarding.get_op(9999) == nil)\n"
        "assert(engine.boarding.get_meltdown(9999) == nil)\n"
        "local ok, _ = pcall(require, 'missing_mod_xyz')\n"
        "assert(not ok)\n",
        "=binding_checks"), vm.last_error);

    /* Error handling branches */
    TEST_ASSERT_FALSE(rts_lua_exec_vfs_script(&vm, "does_not_exist.lua"));
    TEST_ASSERT_FALSE(rts_lua_exec_string(&vm, "error('boom')", "=err_test"));

    rts_lua_enable_coverage(&vm, false);
    rts_lua_shutdown(&vm);
    rts_audio_shutdown(&audio);
}

void register_lua_vm_tests(void) {
    RUN_TEST(test_lua_vm_bindings_sandbox_and_coverage);
}
