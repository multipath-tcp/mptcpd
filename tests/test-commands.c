// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-commands.c
 *
 * @brief mptcpd commands API test.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#undef NDEBUG
#include <assert.h>

#include <ell/main.h>
#include <ell/idle.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/test.h>

#include "../src/configuration.h"   // INTERNAL!
#include "../src/path_manager.h"    // INTERNAL!

#include "test-plugin.h"

#include <mptcpd/path_manager.h>


// -------------------------------------------------------------------

void test_send_addr(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        assert(mptcpd_pm_send_addr(pm,
                                   test_cid_1,
                                   test_laddr_id_1,
                                   &test_laddr_1));
}

void test_add_subflow(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        assert(mptcpd_pm_add_subflow(pm,
                                     test_cid_2,
                                     test_laddr_id_2,
                                     test_raddr_id_2,
                                     &test_laddr_2,
                                     &test_raddr_2,
                                     test_backup_2));
}

void test_set_backup(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        assert(mptcpd_pm_set_backup(pm,
                                    test_cid_1,
                                    &test_laddr_1,
                                    &test_raddr_1,
                                    test_backup_1));
}

void test_remove_subflow(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        assert(mptcpd_pm_remove_subflow(pm,
                                        test_cid_1,
                                        &test_laddr_1,
                                        &test_raddr_1));
}

// -------------------------------------------------------------------

static void idle_callback(struct l_idle *idle, void *user_data)
{
        (void) idle;
        (void) user_data;

        /*
          Number of ELL event loop iterations to go through before
          triggering the MPTCP path management generic netlink command
          calls.

          This gives the mptcpd generic netlink API implementation
          enough time for the "mptcp" generic netlink family to
          "appear" over several ELL event loop iterations, as well as
          the generic netlink commands to be sent.
        */
        static int const trigger_count = 10;

        /*
          Maximum number of ELL event loop iterations.

          Stop the ELL event loop after this number of iterations.
         */
        static int const max_count = trigger_count * 2;

        // ELL event loop iteration count.
        static int count = 0;

        static bool tests_called = false;

        assert(max_count > trigger_count);  // Sanity check.

        /*
          The mptcpd network monitor interface list should now be
          populated.  Iterate through the list, and verify that the
          monitored interfaces are what we expect.
         */
        if (count > trigger_count && !tests_called) {
                l_test_run();
                tests_called = true;
        }

        if (++count > max_count)
                l_main_quit();
}

// -------------------------------------------------------------------

int main(void)
{
        if (!l_main_init())
                return -1;

        l_log_set_stderr();
        l_debug_enable("*");

        static char *argv[] = {
                "test-commands",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };
        static char **args = argv;


        static int argc = L_ARRAY_SIZE(argv);

        struct mptcpd_config *const config =
                mptcpd_config_create(argc, argv);
        assert(config);

        struct mptcpd_pm *pm = mptcpd_pm_create(config);
        assert(pm);

        l_test_init(&argc, &args);

        l_test_add("send_addr",      test_send_addr,      pm);
        l_test_add("add_subflow",    test_add_subflow,    pm);
        l_test_add("set_backup",     test_set_backup,     pm);
        l_test_add("remove_subflow", test_remove_subflow, pm);

        /*
          Prepare to run the path management generic netlink command
          tests.
        */
        struct l_idle *const idle =
                l_idle_create(idle_callback, &pm, NULL);

        (void) l_main_run();

        mptcpd_pm_destroy(pm);
        mptcpd_config_destroy(config);

        l_idle_remove(idle);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
