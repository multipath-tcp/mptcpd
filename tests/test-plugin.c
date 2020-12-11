// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-plugin.c
 *
 * @brief mptcpd plugin test.
 *
 * Copyright (c) 2018-2020, Intel Corporation
 */

#undef NDEBUG

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>

#include <ell/test.h>

#include <mptcpd/plugin.h>
#include <mptcpd/plugin_private.h>

#include "test-plugin.h"


static bool run_plugin_load(mode_t mode)
{
        static char const dir[]            = TEST_PLUGIN_DIR_SECURITY;
        static char const default_plugin[] = TEST_PLUGIN_FOUR;
        struct mptcpd_pm *const pm         = NULL;

        int const fd = open(dir, O_DIRECTORY);
        assert(fd != -1);

        struct stat st;
        int const stat_ok = fstat(fd, &st);
        assert(stat_ok == 0);

        int const mode_ok = fchmod(fd, mode);
        assert(mode_ok == 0);

        bool const loaded = mptcpd_plugin_load(dir, default_plugin, pm);

        if (loaded) {
                call_plugin_ops(&test_count_4,
                                NULL,
                                test_token_4,
                                test_raddr_id_4,
                                (struct sockaddr const *) &test_laddr_4,
                                (struct sockaddr const *) &test_raddr_4,
                                test_backup_4);

                mptcpd_plugin_unload(pm);
        }

        (void) fchmod(fd, st.st_mode);  // Restore original perms.

        (void) close(fd);

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

        struct mptcpd_pm *const pm = NULL;
        bool const loaded = mptcpd_plugin_load(dir, NULL, pm);

        (void) rmdir(dir);

        assert(!loaded);
}

/**
 * @brief Verify dispatch of plugin operations.
 *
 * Confirm that specific plugins are dispatched with the expected
 * arguments.
 */
static void test_plugin_dispatch(void const *test_data)
{
        (void) test_data;

        static char const dir[] = TEST_PLUGIN_DIR_PRIORITY;
        static char const *const default_plugin = NULL;
        struct mptcpd_pm *const pm = NULL;

        bool const loaded = mptcpd_plugin_load(dir, default_plugin, pm);
        assert(loaded);

        // Notice that we call plugin 1 twice.
        // Plugin 1
        call_plugin_ops(&test_count_1,
                        TEST_PLUGIN_ONE,
                        test_token_1,
                        test_raddr_id_1,
                        (struct sockaddr const *) &test_laddr_1,
                        (struct sockaddr const *) &test_raddr_1,
                        test_backup_1);

        // Plugin 1 as default
        call_plugin_ops(&test_count_1,
                        NULL,
                        test_token_1,
                        test_raddr_id_1,
                        (struct sockaddr const *) &test_laddr_1,
                        (struct sockaddr const *) &test_raddr_1,
                        test_backup_1);

        // Plugin 2
        call_plugin_ops(&test_count_2,
                        TEST_PLUGIN_TWO,
                        test_token_2,
                        test_raddr_id_2,
                        (struct sockaddr const *) &test_laddr_2,
                        (struct sockaddr const *) &test_raddr_2,
                        test_backup_2);

        /*
          Invalid MPTCP token - no plugin dispatch should occur.

          This should be the case for all operations corresponding to
          MPTCP genl API events that are triggered after the initial
          new connection (MPTCP_CONNECTION_CREATED) event,
          i.e. connection_established, connection_closed,
          new_address, address_removed, new_subflow, subflow_closed,
          and subflow_priority.

          We do not call call_plugin_ops() like above since that would
          cause the token to be associated with a plugin, which is not
          what we want in this case.
        */
        mptcpd_plugin_connection_established(
                test_bad_token,
                (struct sockaddr const *) &test_laddr_2,
                (struct sockaddr const *) &test_raddr_2,
                NULL);

        // Test assertions will be triggered during plugin unload.
        mptcpd_plugin_unload(pm);
}

/**
 * @brief Verify graceful handling of NULL plugin operations.
 *
 * Confirm that the mptcpd plugin framework will not call operations
 * that have not been specified by the user, i.e. those that are
 * @c NULL.
 */
static void test_null_plugin_ops(void const *test_data)
{
        (void) test_data;

        static char const        dir[]          = TEST_PLUGIN_DIR_NOOP;
        static char const *const default_plugin = NULL;
        struct mptcpd_pm *const pm = NULL;

        bool const loaded = mptcpd_plugin_load(dir, default_plugin, pm);
        assert(loaded);

        char const name[] = "null ops";
        struct mptcpd_plugin_ops const ops = { .new_connection = NULL };

        bool registered = mptcpd_plugin_register_ops(name, &ops);
        assert(registered);

        // Unused dummy arguments.
        static mptcpd_token_t const token = 0;
        static mptcpd_aid_t const id = 0;
        static struct sockaddr const *const laddr = NULL;
        static struct sockaddr const *const raddr = NULL;
        static bool backup = false;

        // No dispatch should occur in the following calls.
        mptcpd_plugin_new_connection(name, token, laddr, raddr, pm);
        mptcpd_plugin_connection_established(token, laddr, raddr, pm);
        mptcpd_plugin_connection_closed(token, pm);
        mptcpd_plugin_new_address(token, id, raddr, pm);
        mptcpd_plugin_address_removed(token, id, pm);
        mptcpd_plugin_new_subflow(token, laddr, raddr, backup, pm);
        mptcpd_plugin_subflow_closed(token, laddr, raddr, backup, pm);
        mptcpd_plugin_subflow_priority(token, laddr, raddr, backup, pm);

        mptcpd_plugin_unload(pm);
}

int main(int argc, char *argv[])
{
        l_test_init(&argc, &argv);

        l_test_add("good permissions", test_good_perms,      NULL);
        l_test_add("bad  permissions", test_bad_perms,       NULL);
        l_test_add("no plugins",       test_no_plugins,      NULL);
        l_test_add("plugin dispatch",  test_plugin_dispatch, NULL);
        l_test_add("null plugin ops",  test_null_plugin_ops, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
