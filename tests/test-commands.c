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

#include <linux/mptcp.h>

#include <ell/main.h>
#include <ell/genl.h>
#include <ell/timeout.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/test.h>

#include "../src/configuration.h"   // INTERNAL!
#include "../src/path_manager.h"    // INTERNAL!

#include "test-plugin.h"

#include <mptcpd/path_manager.h>


// -------------------------------------------------------------------

static bool is_pm_ready(struct mptcpd_pm const *pm, char const *fname)
{
        bool const ready = mptcpd_pm_ready(pm);

        if (!ready)
                l_warn("Path manager not yet ready.  "
                       "%s cannot be completed.", fname);

        return ready;
}

// -------------------------------------------------------------------

void test_send_addr(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        if (!is_pm_ready(pm, __func__))
                return;

        assert(mptcpd_pm_send_addr(pm,
                                   test_token_1,
                                   test_laddr_id_1,
                                   &test_laddr_1));
}

void test_add_subflow(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        if (!is_pm_ready(pm, __func__))
                return;

        assert(mptcpd_pm_add_subflow(pm,
                                     test_token_2,
                                     test_laddr_id_2,
                                     test_raddr_id_2,
                                     &test_laddr_2,
                                     &test_raddr_2,
                                     test_backup_2));
}

void test_set_backup(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        if (!is_pm_ready(pm, __func__))
                return;

        assert(mptcpd_pm_set_backup(pm,
                                    test_token_1,
                                    &test_laddr_1,
                                    &test_raddr_1,
                                    test_backup_1));
}

void test_remove_subflow(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        if (!is_pm_ready(pm, __func__))
                return;

        assert(mptcpd_pm_remove_subflow(pm,
                                        test_token_1,
                                        &test_laddr_1,
                                        &test_raddr_1));
}

void test_get_nm(void const *test_data)
{
        struct mptcpd_pm *const pm = (struct mptcpd_pm *) test_data;

        assert(mptcpd_pm_get_nm(pm) != NULL);
}

// -------------------------------------------------------------------

static void run_tests(struct l_genl_family_info const *info,
                      void *user_data)
{
        assert(strcmp(l_genl_family_info_get_name(info),
                      MPTCP_GENL_NAME) == 0);

        l_test_run();

        bool *const tests_called = user_data;
        *tests_called = true;

        l_main_quit();
}

static void timeout_callback(struct l_timeout *timeout,
                             void *user_data)
{
        (void) timeout;
        (void) user_data;

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
        l_test_add("get_nm",         test_get_nm,         pm);

        /*
          Prepare to run the path management generic netlink command
          tests.
        */
        bool tests_called = false;
        struct l_genl *const genl = l_genl_new();
        assert(genl != NULL);

        unsigned int const watch_id =
                l_genl_add_family_watch(genl,
                                        MPTCP_GENL_NAME,
                                        run_tests,
                                        NULL,
                                        &tests_called,
                                        NULL);

        assert(watch_id != 0);

        // Bound the time we wait for the tests to run.
        static unsigned long const milliseconds = 500;
        struct l_timeout *const timeout =
                l_timeout_create_ms(milliseconds,
                                    timeout_callback,
                                    &tests_called,
                                    NULL);

        (void) l_main_run();

        /*
          The tests will have run only if the "mptcp" generic netlink
          family appeared.
         */
        assert(tests_called);

        l_timeout_remove(timeout);
        l_genl_remove_family_watch(genl, watch_id);
        l_genl_unref(genl);
        mptcpd_pm_destroy(pm);
        mptcpd_config_destroy(config);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
