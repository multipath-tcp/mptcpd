// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-plugin.c
 *
 * @brief mptcpd plugin test.
 *
 * Copyright (c) 2018-2022, Intel Corporation
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>

#include <ell/test.h>
#include <ell/queue.h>

#include <mptcpd/plugin.h>
#include <mptcpd/private/plugin.h>

#include "test-plugin.h"

#undef NDEBUG
#include <assert.h>


static bool run_plugin_load(mode_t mode, struct l_queue const *queue)
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

        bool const loaded =
                mptcpd_plugin_load(dir, default_plugin, queue, pm);

        if (loaded) {
                static struct plugin_call_args const args = {
                        .token       = test_token_4,
                        .raddr_id    = test_raddr_id_4,
                        .laddr       = (struct sockaddr const *) &test_laddr_4,
                        .raddr       = (struct sockaddr const *) &test_raddr_4,
                        .backup      = test_backup_4,
                        .server_side = test_server_side_4
                };

                call_plugin_ops(&test_count_4, &args);

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

        bool const loaded = run_plugin_load(mode, NULL);

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

        bool const loaded = run_plugin_load(mode, NULL);

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
        bool const loaded = mptcpd_plugin_load(dir, NULL, NULL, pm);

        (void) rmdir(dir);

        assert(!loaded);
}

/**
 * @brief Verify load of specific existing plugins.
 *
 * Confirm that specific existing plugins are loaded.
 */
static void test_existing_plugins(void const *test_data)
{
        (void) test_data;

        // Owner and group read/write/execute permissions.
        mode_t const mode = S_IRWXU | S_IRWXG;

        struct l_queue *const plugins_list = l_queue_new();

        l_queue_push_tail(plugins_list, "four");

        bool const loaded = run_plugin_load(mode, plugins_list);

        l_queue_destroy(plugins_list, NULL);

        assert(loaded);
}

/**
 * @brief Verify failure if specified plugins do not exist.
 *
 * The @c mptcpd_plugin_load() function should return with a failure
 * if the specified plugins do not exist in the provided plugin directory.
 */
static void test_nonexistent_plugins(void const *test_data)
{
        (void) test_data;

        // Owner and group read/write/execute permissions.
        mode_t const mode = S_IRWXU | S_IRWXG;

        struct l_queue *const plugins_list = l_queue_new();

        static char nonexistent_plugin[] = "nonexistent_plugin";
        l_queue_push_tail(plugins_list, nonexistent_plugin);

        bool const loaded = run_plugin_load(mode, plugins_list);

        l_queue_destroy(plugins_list, NULL);

        /*
          Force plugin name-to-ops lookup to fail excercise additional
          error paths.
        */
        mptcpd_plugin_new_connection(nonexistent_plugin,
                                     0,     // token
                                     NULL,  // laddr
                                     NULL,  // raddr
                                     false, // server_side
                                     NULL); // pm

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

        bool const loaded =
                mptcpd_plugin_load(dir, default_plugin, NULL, pm);
        assert(loaded);

        // Notice that we call plugin 1 twice.
        // Plugin 1
        struct plugin_call_args const args1 = {
                .name        = TEST_PLUGIN_ONE,
                .token       = test_token_1,
                .raddr_id    = test_raddr_id_1,
                .laddr       = (struct sockaddr const *) &test_laddr_1,
                .raddr       = (struct sockaddr const *) &test_raddr_1,
                .backup      = test_backup_1,
                .server_side = test_server_side_1
        };

        call_plugin_ops(&test_count_1, &args1);

        // Plugin 1 as default (no plugin name specified)
        struct plugin_call_args const args1_default = {
                .token       = args1.token,
                .raddr_id    = args1.raddr_id,
                .laddr       = args1.laddr,
                .raddr       = args1.raddr,
                .backup      = args1.backup,
                .server_side = args1.server_side
        };

        call_plugin_ops(&test_count_1, &args1_default);

        // Plugin 2
        struct plugin_call_args const args2 = {
                .name        = TEST_PLUGIN_TWO,
                .token       = test_token_2,
                .raddr_id    = test_raddr_id_2,
                .laddr       = (struct sockaddr const *) &test_laddr_2,
                .raddr       = (struct sockaddr const *) &test_raddr_2,
                .backup      = test_backup_2,
                .server_side = test_server_side_2
        };

        call_plugin_ops(&test_count_2, &args2);

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
                test_server_side_2,
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

        bool const loaded = mptcpd_plugin_load(dir, default_plugin, NULL, pm);
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
        static bool server_side = false;
        static struct mptcpd_interface const *const interface = NULL;

        // No dispatch should occur in the following calls.
        mptcpd_plugin_new_connection(name, token, laddr, raddr, server_side, pm);
        mptcpd_plugin_connection_established(token, laddr, raddr, server_side, pm);
        mptcpd_plugin_connection_closed(token, pm);
        mptcpd_plugin_new_address(token, id, raddr, pm);
        mptcpd_plugin_address_removed(token, id, pm);
        mptcpd_plugin_new_subflow(token, laddr, raddr, backup, pm);
        mptcpd_plugin_subflow_closed(token, laddr, raddr, backup, pm);
        mptcpd_plugin_subflow_priority(token, laddr, raddr, backup, pm);
        mptcpd_plugin_listener_created(name, laddr, pm);
        mptcpd_plugin_listener_closed(name, laddr, pm);
        mptcpd_plugin_new_interface(interface, pm);
        mptcpd_plugin_update_interface(interface, pm);
        mptcpd_plugin_delete_interface(interface, pm);
        mptcpd_plugin_new_local_address(interface, laddr, pm);
        mptcpd_plugin_delete_local_address(interface, laddr, pm);

        mptcpd_plugin_unload(pm);
}

/**
 * @brief Verify graceful handling of @c NULL plugin directory.
 */
static void test_null_plugin_dir(void const *test_data)
{
        (void) test_data;

        char const *const dir                       = NULL;
        char const *const default_plugin            = NULL;
        struct l_queue const *const plugins_to_load = NULL;
        struct mptcpd_pm *const pm                  = NULL;

        bool const loaded =
                mptcpd_plugin_load(dir,
                                   default_plugin,
                                   plugins_to_load,
                                   pm);
        assert(!loaded);
}

/**
 * @brief Verify graceful handling of bad/malformed plugins.
 */
static void test_bad_plugins(void const *test_data)
{
        (void) test_data;

        char const *const dir                       = TEST_PLUGIN_DIR_BAD;
        char const *const default_plugin            = NULL;
        struct l_queue const *const plugins_to_load = NULL;
        struct mptcpd_pm *const pm                  = NULL;

        bool const loaded =
                mptcpd_plugin_load(dir,
                                   default_plugin,
                                   plugins_to_load,
                                   pm);
        assert(!loaded);
}

int main(int argc, char *argv[])
{
        l_test_init(&argc, &argv);

        l_test_add("good permissions",   test_good_perms,          NULL);
        l_test_add("bad  permissions",   test_bad_perms,           NULL);
        l_test_add("no plugins",         test_no_plugins,          NULL);
        l_test_add("existing plugin",    test_existing_plugins,    NULL);
        l_test_add("nonexistent plugin", test_nonexistent_plugins, NULL);
        l_test_add("plugin dispatch",    test_plugin_dispatch,     NULL);
        l_test_add("null plugin ops",    test_null_plugin_ops,     NULL);
        l_test_add("null plugin dir",    test_null_plugin_dir,     NULL);
        l_test_add("bad plugins",        test_bad_plugins,         NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
