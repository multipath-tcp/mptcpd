// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-plugin.c
 *
 * @brief mptcpd plugin test.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#undef NDEBUG

#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <ell/test.h>

#include <mptcpd/plugin.h>

#include "test-plugin.h"


static bool run_plugin_load(mode_t mode)
{
        static char const dir[]            = TEST_PLUGIN_DIR_A;
        static char const default_plugin[] = TEST_PLUGIN_FOUR;

        struct stat st;
        int const stat_ok = stat(dir, &st);
        assert(stat_ok == 0);

        int const mode_ok = chmod(dir, mode);
        assert(mode_ok == 0);

        bool const loaded = mptcpd_plugin_load(dir, default_plugin);

        if (loaded) {
                call_plugin_ops(&test_count_4,
                                NULL,
                                test_token_4,
                                test_laddr_id_4,
                                test_raddr_id_4,
                                &test_laddr_4,
                                &test_raddr_4,
                                test_backup_4);

                mptcpd_plugin_unload();
        }

        (void) chmod(dir, st.st_mode);  // Restore original perms.

        return loaded;
}

/**
 * @brief Verify good plugin directory permissions.
 *
 * Confirm that owner and group read/write/execute permissions on a
 * given plugin directory are accepted.
 */
static void test_good_perms(void const *test_data)
{
        (void) test_data;

        // Owner and group read/write/execute permissions.
        mode_t const mode = S_IRWXU | S_IRWXG;

        bool const loaded = run_plugin_load(mode);

        assert(loaded);
}

/**
 * @brief Verify bad plugin directory permissions.
 *
 * Confirm that "other" write permissions on a given plugin directory
 * are rejected.
 */
static void test_bad_perms(void const *test_data)
{
        (void) test_data;

        // Owner, group, and other read/write/execute permissions.
        mode_t const mode = S_IRWXU | S_IRWXG | S_IRWXO;

        bool const loaded = run_plugin_load(mode);

        // Write permissions for "other" should be rejected.
        assert(!loaded);
}

/**
 * @brief Verify failure if no plugins exist.
 *
 * The @c mptcpd_plugin_load() function should return with a failure
 * if no plugins exist in the provided plugin directory.
 */
static void test_no_plugins(void const *test_data)
{
        (void) test_data;

        // Create an empty directory.
        char template[]       = "/tmp/testdir.XXXXXX";
        char const *const dir = mkdtemp(template);

        assert(dir != NULL);

        bool const loaded = mptcpd_plugin_load(dir, NULL);
        assert(!loaded);
}

/**
 * @brief Verify dispatch of plugin operations.
 *
 *
 */
static void test_plugin_dispatch(void const *test_data)
{
        (void) test_data;

        static char const        dir[]          = TEST_PLUGIN_DIR_B;
        static char const *const default_plugin = NULL;

        bool const loaded = mptcpd_plugin_load(dir, default_plugin);
        assert(loaded);

        // Notice that we call plugin 1 twice.
        // Plugin 1
        call_plugin_ops(&test_count_1,
                        TEST_PLUGIN_ONE,
                        test_token_1,
                        test_laddr_id_1,
                        test_raddr_id_1,
                        &test_laddr_1,
                        &test_raddr_1,
                        test_backup_1);

        // Plugin 1 as default
        call_plugin_ops(&test_count_1,
                        NULL,
                        test_token_1,
                        test_laddr_id_1,
                        test_raddr_id_1,
                        &test_laddr_1,
                        &test_raddr_1,
                        test_backup_1);

        // Plugin 2
        call_plugin_ops(&test_count_2,
                        TEST_PLUGIN_TWO,
                        test_token_2,
                        test_laddr_id_2,
                        test_raddr_id_2,
                        &test_laddr_2,
                        &test_raddr_2,
                        test_backup_2);

        // Test assertions will be triggered during plugin unload.
        mptcpd_plugin_unload();
}

int main(int argc, char *argv[])
{
        l_test_init(&argc, &argv);

        l_test_add("good permissions", test_good_perms,      NULL);
        l_test_add("bad  permissions", test_bad_perms,       NULL);
        l_test_add("no plugins",       test_no_plugins,      NULL);
        l_test_add("plugin dispatch",  test_plugin_dispatch, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
