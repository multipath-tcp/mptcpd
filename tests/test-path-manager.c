// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-path-manager.c
 *
 * @brief mptcpd path manager test.
 *
 * Copyright (c) 2019-2021, Intel Corporation
 */

#include <unistd.h>

#include <ell/ell.h>

#include "test-util.h"

#include "../src/path_manager.h"           // INTERNAL!
#include <mptcpd/private/configuration.h>  // INTERNAL!
#include <mptcpd/private/path_manager.h>   // INTERNAL!
#include <mptcpd/path_manager.h>

#undef NDEBUG
#include <assert.h>


// -------------------------------------------------------------------

struct test_info
{
        struct mptcpd_config *const config;

        char const *const family_name;

        struct mptcpd_pm *pm;

        bool internals_check_called;
        bool pm_ready_called;
        bool pm_not_ready_called;
};

// -------------------------------------------------------------------

static void pm_ready(struct mptcpd_pm *pm, void *user_data)
{
        (void) pm;

        struct test_info *const info = (struct test_info *) user_data;

        info->pm_ready_called = true;

        l_main_quit();
}

static void pm_not_ready(struct mptcpd_pm *pm, void *user_data)
{
        (void) pm;

        /*
          This callback should only be triggered if MPTCP generic
          netlink family is not available.  Either the netlink path
          manager in the kernel was enabled, or MPTCP is not supported
          by the kernel.
         */
        struct test_info *const info = (struct test_info *) user_data;

        info->pm_not_ready_called = true;

        l_main_quit();
}

static struct mptcpd_pm_ops const pm_ops = {
        .ready     = pm_ready,
        .not_ready = pm_not_ready
};

static struct mptcpd_pm_ops const bad_pm_ops = {
        .ready     = NULL,
        .not_ready = NULL
};

// -------------------------------------------------------------------

static void test_pm_create(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        struct mptcpd_pm *const pm = mptcpd_pm_create(info->config);

        assert(pm            != NULL);
        assert(pm->config    != NULL);
        assert(pm->genl      != NULL);
        assert(pm->timeout   != NULL);
        assert(pm->nm        != NULL);
        assert(pm->idm       != NULL);
        assert(pm->lm        != NULL);
        assert(pm->event_ops != NULL);

        assert(mptcpd_pm_get_nm(pm)  == pm->nm);
        assert(mptcpd_pm_get_idm(pm) == pm->idm);
        assert(mptcpd_pm_get_lm(pm)  == pm->lm);

        /*
          Other struct mptcpd_pm fields may not have been initialized
          yet since they depend on the existence of the "mptcp"
          generic netlink family.
        */

        info->pm = pm;
}

static void test_pm_register_ops(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;

        bool const registered =
                mptcpd_pm_register_ops(info->pm, &pm_ops, info);

        bool const not_registered =
                !mptcpd_pm_register_ops(info->pm, &bad_pm_ops, info);

        assert(registered);
        assert(not_registered);
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

static void test_pm_internals(struct l_genl_family_info const *info,
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

        t->internals_check_called = true;
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
        // Skip this test if the kernel is not MPTCP capable.
        tests_skip_if_no_mptcp();

        if (!l_main_init())
                return -1;

        l_log_set_stderr();
        l_debug_enable("*");

        static char *argv[] = {
                "test-path-manager",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };

        static int argc = L_ARRAY_SIZE(argv);

        struct test_info info = {
                .config = mptcpd_config_create(argc, argv),
                .family_name = tests_get_pm_family_name()
        };

        assert(info.config);
        assert(info.family_name);

        test_pm_create(&info);
        test_pm_register_ops(&info);

        /*
          Run the path manager lifecycle tests when the "mptcp"
          generic netlink family appears.
        */
        struct l_genl *const genl = l_genl_new();
        assert(genl != NULL);

        unsigned int const watch_id =
                l_genl_add_family_watch(genl,
                                        info.family_name,
                                        test_pm_internals,
                                        NULL,
                                        &info,
                                        NULL);

        assert(watch_id != 0);

        bool const requested = l_genl_request_family(genl,
                                                     info.family_name,
                                                     test_pm_internals,
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

        test_pm_destroy(&info);

        /*
          The tests will have run only if the "mptcp" generic netlink
          family appeared.
         */
        assert(info.internals_check_called);
        assert(info.pm_ready_called);
        assert(!info.pm_not_ready_called);

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
