// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-path-manager.c
 *
 * @brief mptcpd path manager test.
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

#include "../src/configuration.h"        // INTERNAL!
#include "../src/path_manager.h"         // INTERNAL!
#include <mptcpd/path_manager_private.h> // INTERNAL!

// -------------------------------------------------------------------

struct test_info
{
        struct mptcpd_config *config;

        struct mptcpd_pm *pm;
};

// -------------------------------------------------------------------

void test_pm_create(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        info->pm = mptcpd_pm_create(info->config);

        assert(info->pm         != NULL);
        assert(info->pm->genl   != NULL);
        assert(info->pm->family != NULL);
        assert(info->pm->family != NULL);
        assert(info->pm->id     != NULL);
        assert(info->pm->nm     != NULL);
}

void test_pm_destroy(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        mptcpd_pm_destroy(info->pm);
}

// -------------------------------------------------------------------

static void idle_callback(struct l_idle *idle, void *user_data)
{
        (void) idle;
        (void) user_data;

        /*
          Number of ELL event loop iterations to go through before
          triggering the MPTCP path manager tests.

          This gives the mptcpd generic netlink API implementation
          enough time for the "mptcp" generic netlink family to
          "appear" over several ELL event loop iterations.
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
                "test-path-manager",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };
        static char **args = argv;

        static int argc = L_ARRAY_SIZE(argv);

        struct test_info info = { NULL };

        info.config = mptcpd_config_create(argc, argv);
        assert(info.config);

        l_test_init(&argc, &args);

        l_test_add("pm_create",  test_pm_create,  &info);
        l_test_add("pm_destroy", test_pm_destroy, &info);

        // Prepare to run the path manager lifecycle tests.
        struct l_idle *const idle =
                l_idle_create(idle_callback, &info, NULL);

        (void) l_main_run();

        mptcpd_config_destroy(info.config);

        l_idle_remove(idle);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
