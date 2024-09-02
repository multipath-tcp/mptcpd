// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-sspi.c
 *
 * @brief Test the single-subflow-per-interface path manager plugin.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include "../src/path_manager.h"    // INTERNAL!
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>
#include <mptcpd/private/configuration.h>
#include <mptcpd/private/plugin.h>

#include <ell/main.h>
#include <ell/timeout.h>
#include <ell/util.h>
#include <ell/log.h>
#include <ell/test.h>

#include "test-plugin.h"
#include "test-util.h"
#include "interface.h"

#undef NDEBUG
#include <assert.h>
#include <stdbool.h>


struct nm_event_args
{
        struct mptcpd_interface const *const interface;
        struct sockaddr const *const addr;
        struct mptcpd_pm *pm;
};

// -------------------------------------------------------------------
//                       sspi plugin test cases
// -------------------------------------------------------------------
static void test_new_connection(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        static bool const server_side = false;

        mptcpd_plugin_new_connection(args->name,
                                     args->token,
                                     args->laddr,
                                     args->raddr,
                                     server_side,
                                     args->pm);
}

static void test_connection_established(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        static bool const server_side = false;

        mptcpd_plugin_connection_established(args->token,
                                             args->laddr,
                                             args->raddr,
                                             server_side,
                                             args->pm);
}

static void test_connection_closed(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        mptcpd_plugin_connection_closed(args->token, args->pm);
}

static void test_new_subflow(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        mptcpd_plugin_new_subflow(args->token,
                                  args->laddr,
                                  args->raddr,
                                  args->backup,
                                  args->pm);
}

static void test_subflow_closed(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        mptcpd_plugin_subflow_closed(args->token,
                                     args->laddr,
                                     args->raddr,
                                     args->backup,
                                     args->pm);
}

static void test_subflow_priority(void const *test_data)
{
        struct plugin_call_args *const args =
                (struct plugin_call_args *) test_data;

        mptcpd_plugin_subflow_priority(args->token,
                                       args->laddr,
                                       args->raddr,
                                       args->backup,
                                       args->pm);
}

static void test_new_local_address(void const *test_data)
{
        struct nm_event_args *const args =
                (struct nm_event_args *) test_data;

        mptcpd_plugin_new_local_address(
                args->interface,
                args->addr,
                args->pm);
}

static void test_delete_local_address(void const *test_data)
{
        struct nm_event_args *const args =
                (struct nm_event_args *) test_data;

        mptcpd_plugin_delete_local_address(
                args->interface,
                args->addr,
                args->pm);
}

// -------------------------------------------------------------------

static void timeout_callback(struct l_timeout *timeout,
                             void *user_data)
{
        (void) timeout;
        (void) user_data;

        l_debug("test timed out");

        l_main_quit();
}

static void pm_ready(struct mptcpd_pm *pm, void *user_data)
{
        (void) user_data;

        bool const loaded = mptcpd_plugin_load(TEST_PLUGIN_DIR,
                                               NULL,
                                               NULL,
                                               pm);
        assert(loaded);

        l_test_run();
}

static struct mptcpd_pm_ops const pm_ops = { .ready = pm_ready };

int main(void)
{
        // Skip this test if the kernel is not MPTCP capable.
        tests_skip_if_no_mptcp();

        if (!l_main_init())
                return -1;

        l_log_set_stderr();
        l_debug_enable("*");

        static char *argv[] = {
                "test-sspi",
                "--load-plugins=sspi",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };

        static int argc = L_ARRAY_SIZE(argv);

        struct mptcpd_config *const config =
                mptcpd_config_create(argc, argv);
        assert(config != NULL);

        struct mptcpd_pm *const pm = mptcpd_pm_create(config);
        assert(pm != NULL);

        bool const registered = mptcpd_pm_register_ops(pm, &pm_ops, NULL);
        assert(registered);

        /*
          Bound the time we wait for the tests to run.
        */
        static unsigned long const milliseconds = 500;
        struct l_timeout *const timeout =
                l_timeout_create_ms(milliseconds,
                                    timeout_callback,
                                    NULL,
                                    NULL);

        l_test_init(&argc,  (char ***) &argv);

        // MPTCP connection event related arguments.
        struct plugin_call_args conn_args = {
                .name = "sspi",
                .token = test_token_1,
                .laddr = (struct sockaddr const *) &test_laddr_2,
                .raddr = (struct sockaddr const *) &test_raddr_1,
                .pm = pm
        };

        // MPTCP subflow event related arguments.
        struct plugin_call_args sub_args = {
                .token = test_token_1,
                .laddr = (struct sockaddr const *) &test_laddr_4,
                .raddr = (struct sockaddr const *) &test_raddr_4,
                .backup = test_backup_4,
                .pm = pm
        };

        // MPTCP local address network monitoring event related
        // arguments.
        struct mptcpd_interface *const i = test_mptcpd_interface_create();
        struct sockaddr const *const addr =
                (struct sockaddr const *) &test_laddr_1;
        bool const inserted = test_mptcpd_interface_insert_addr(i, addr);
        assert(inserted);

        struct nm_event_args nm_args = {
                .interface = i,
                .addr = addr,
                .pm = pm
        };

        l_test_add("new_connection", test_new_connection, &conn_args);
        l_test_add("connection_established",
                   test_connection_established,
                   &conn_args);
        l_test_add("connection_closed",
                   test_connection_closed,
                   &conn_args);
        l_test_add("new_subflow", test_new_subflow, &sub_args);
        l_test_add("subflow_closed", test_subflow_closed, &sub_args);
        l_test_add("subflow_priority", test_subflow_priority, &sub_args);
        l_test_add("new_local_address", test_new_local_address, &nm_args);
        l_test_add("delete_local_address",
                   test_delete_local_address,
                   &nm_args);

        (void) l_main_run();

        test_mptcpd_interface_destroy(i);

        mptcpd_plugin_unload(pm);
        l_timeout_remove(timeout);
        mptcpd_pm_destroy(pm);
        mptcpd_config_destroy(config);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
