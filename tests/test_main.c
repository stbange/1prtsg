/* SPDX-License-Identifier: MIT */
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void register_math3d_tests(void);
void register_planet_tests(void);
void register_vfs_tests(void);
void register_interior_tests(void);
void register_world_tests(void);
void register_voxel_tests(void);
void register_input_tests(void);
void register_audio_tests(void);
void register_economy_tests(void);
void register_controller_tests(void);
void register_boarding_tests(void);
void register_assets_and_renderer_tests(void);
void register_lua_vm_tests(void);

int main(void) {
    UNITY_BEGIN();
    register_math3d_tests();
    register_planet_tests();
    register_vfs_tests();
    register_interior_tests();
    register_world_tests();
    register_voxel_tests();
    register_input_tests();
    register_audio_tests();
    register_economy_tests();
    register_controller_tests();
    register_boarding_tests();
    register_assets_and_renderer_tests();
    register_lua_vm_tests();
    return UNITY_END();
}
