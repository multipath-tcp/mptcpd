// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-path-manager.c
 *
 * @brief mptcpd path manager test.
 *
 * Copyright (c) 2019-2021, Intel Corporation
 */

#undef NDEBUG
#include <assert.h>

#include <unistd.h>

#include <ell/main.h>
#include <ell/genl.h>
#include <ell/timeout.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/test.h>

#include "test-util.h"

#include "../src/configuration.h"        // INTERNAL!
#include "../src/path_manager.h"         // INTERNAL!
#include <mptcpd/private/path_manager.h> // INTERNAL!
#include <mptcpd/path_manager.h>

// -------------------------------------------------------------------

struct test_info
{
        struct mptcpd_config *const config;

        char const *const family_name;

        struct mptcpd_pm *pm;

        bool tests_called;
};

// -------------------------------------------------------------------

static void test_pm_create(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        info->pm = mptcpd_pm_create(info->config);

        assert(info->pm       != NULL);
        assert(info->pm->genl != NULL);
        assert(info->pm->nm   != NULL);

        /*
          Other struct mptcpd_pm fields may not have been initialized
          yet since they depend on the existence of the "mptcp"
          generic netlink family.
        */
}

static void test_pm_destroy(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        if (!mptcpd_pm_ready(info->pm))
                l_warn("Path manager was not ready.  "
                       "Test was likely limited.");

        mptcpd_pm_destroy(info->pm);
}

// -------------------------------------------------------------------

static void run_tests(struct l_genl_family_info const *info,
                      void *user_data)
{
        /*
          Check if the initial request for the MPTCP generic netlink
          family failed.  A subsequent family watch will be used to
          call this function again when it appears.
         */
        if (info == NULL)
                return;

        struct test_info *const t = user_data;

        assert(strcmp(l_genl_family_info_get_name(info),
                      t->family_name) == 0);

        l_test_run();

        t->tests_called = true;

        l_main_quit();
}

static void timeout_callback(struct l_timeout *timeout,
                             void *user_data)
{
        (void) timeout;
        (void) user_data;

        l_debug("test timed out");

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
                "test-path-manager",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };
        static char **args = argv;

        static int argc = L_ARRAY_SIZE(argv);

        struct test_info info = {
                .config = mptcpd_config_create(argc, argv),
                .family_name = tests_get_pm_family_name()
        };

        assert(info.config);
        assert(info.family_name);

        l_test_init(&argc, &args);

        l_test_add("pm_create",  test_pm_create,  &info);
        l_test_add("pm_destroy", test_pm_destroy, &info);

        /*
          Run the path manager lifecycle tests when the "mptcp"
          generic netlink family appears.
        */
        struct l_genl *const genl = l_genl_new();
        assert(genl != NULL);

        unsigned int const watch_id =
                l_genl_add_family_watch(genl,
                                        info.family_name,
                                        run_tests,
                                        NULL,
                                        &info,
                                        NULL);

        assert(watch_id != 0);

        bool const requested = l_genl_request_family(genl,
                                                     info.family_name,
                                                     run_tests,
                                                     &info,
                                                     NULL);
        assert(requested);

        // Bound the time we wait for the tests to run.
        static unsigned long const milliseconds = 500;
        struct l_timeout *const timeout =
                l_timeout_create_ms(milliseconds,
                                    timeout_callback,
                                    NULL,
                                    NULL);

        (void) l_main_run();

        /*
          The tests will have run only if the "mptcp" generic netlink
          family appeared.
         */
        assert(info.tests_called);

        l_timeout_remove(timeout);
        l_genl_remove_family_watch(genl, watch_id);
        l_genl_unref(genl);
        mptcpd_config_destroy(info.config);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
