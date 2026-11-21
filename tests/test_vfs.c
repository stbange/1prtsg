/* SPDX-License-Identifier: MIT */
#include "rts/vfs.h"
#include "unity.h"
#include <stdlib.h>
#include <string.h>

static void test_vfs_path_sandboxing(void) {
    TEST_ASSERT_TRUE(rts_vfs_validate_path("units/buildings.lua"));
    TEST_ASSERT_TRUE(rts_vfs_validate_path("textures/hull_blue.png"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path(NULL));
    TEST_ASSERT_FALSE(rts_vfs_validate_path(""));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("/etc/passwd"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("\\windows\\sys"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("C:secret.txt"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("../escape.lua"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("units/../../escape.lua"));
    TEST_ASSERT_FALSE(rts_vfs_validate_path("units\\bad.lua"));
}

static void test_vfs_real_external_files_and_mod_override(void) {
    RtsVfs vfs;
    TEST_ASSERT_FALSE(rts_vfs_init(&vfs, "", NULL));
    char trailing[512];
    snprintf(trailing, sizeof(trailing), "%s/", RTS_TEST_DATA_DIR);
    TEST_ASSERT_TRUE(rts_vfs_init(&vfs, trailing, NULL));

    TEST_ASSERT_TRUE(rts_vfs_exists(&vfs, "scripts", "main.lua"));
    TEST_ASSERT_TRUE(rts_vfs_exists(&vfs, "assets", "textures/hull_blue.png"));
    TEST_ASSERT_FALSE(rts_vfs_exists(&vfs, "scripts", "does_not_exist.lua"));
    TEST_ASSERT_FALSE(rts_vfs_exists(&vfs, "scripts", "../README.md"));

    TEST_ASSERT_TRUE(rts_vfs_mtime(&vfs, "scripts", "main.lua") > 0u);
    TEST_ASSERT_EQUAL_UINT64(0u, rts_vfs_mtime(&vfs, "scripts", "missing.lua"));

    size_t len = 0;
    char *base_txt = rts_vfs_read_text(&vfs, "scripts", "units/ground_vehicles.lua", &len);
    TEST_ASSERT_NOT_NULL(base_txt);
    TEST_ASSERT_TRUE(len > 100);
    TEST_ASSERT_NULL(strstr(base_txt, "Emerald Vanguard Plasma Tank"));
    rts_vfs_free(base_txt);
    TEST_ASSERT_NULL(rts_vfs_read_text(&vfs, "scripts", "missing.lua", NULL));

    /* Mount mods/example_mod and verify layered override precedence */
    TEST_ASSERT_FALSE(rts_vfs_mount_mod(&vfs, "nonexistent_mod"));
    TEST_ASSERT_FALSE(rts_vfs_mount_mod(&vfs, "../bad_mod"));
    TEST_ASSERT_TRUE(rts_vfs_mount_mod(&vfs, "example_mod"));
    TEST_ASSERT_EQUAL_UINT(1u, vfs.mod_count);
    TEST_ASSERT_EQUAL_STRING("Emerald Vanguard Overhaul", vfs.mods[0].name);

    char *mod_txt = rts_vfs_read_text(&vfs, "scripts", "units/ground_vehicles.lua", NULL);
    TEST_ASSERT_NOT_NULL(mod_txt);
    TEST_ASSERT_NOT_NULL(strstr(mod_txt, "Emerald Vanguard Plasma Tank"));
    rts_vfs_free(mod_txt);

    /* Disabled mod falls back to base */
    vfs.mods[0].enabled = false;
    char resolved[RTS_VFS_MAX_PATH];
    TEST_ASSERT_TRUE(rts_vfs_resolve(&vfs, "scripts", "units/ground_vehicles.lua", resolved, sizeof(resolved)));
    TEST_ASSERT_NULL(strstr(resolved, "example_mod"));
    /* Buffer too small fails cleanly */
    TEST_ASSERT_FALSE(rts_vfs_resolve(&vfs, "scripts", "units/ground_vehicles.lua", resolved, 4));
}

void register_vfs_tests(void) {
    RUN_TEST(test_vfs_path_sandboxing);
    RUN_TEST(test_vfs_real_external_files_and_mod_override);
}
