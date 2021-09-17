// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-configuration.c
 *
 * @brief mptcpd configuration test.
 *
 * Copyright (c) 2019, 2021, Intel Corporation
 */

#undef NDEBUG
#include <assert.h>

#include <ell/main.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/test.h>

#include <mptcpd/private/configuration.h>  // INTERNAL!


#define TEST_PROGRAM_NAME "test-configuration"

#define RUN_CONFIG(argv) run_config(L_ARRAY_SIZE(argv), (argv))

static void run_config(int argc, char **argv)
{
        struct mptcpd_config *const config =
                mptcpd_config_create(argc, argv);
        assert(config != NULL);

        mptcpd_config_destroy(config);

        l_log_set_stderr();
}

static void test_debug(void const *test_data)
{
        (void) test_data;

        static char *argv1[] = { TEST_PROGRAM_NAME, "--debug" };
        static char *argv2[] = { TEST_PROGRAM_NAME, "-d" };

        RUN_CONFIG(argv1);
        RUN_CONFIG(argv2);
}


#define TEST_LOG(logger)                                                \
        do {                                                            \
                static char *argv1[] = { TEST_PROGRAM_NAME, "--log=" logger }; \
                static char *argv2[] = { TEST_PROGRAM_NAME, "-l", logger }; \
                                                                        \
                RUN_CONFIG(argv1);                                      \
                RUN_CONFIG(argv2);                                      \
        } while(0)

static void test_log_stderr(void const *test_data)
{
        (void) test_data;

        TEST_LOG("stderr");
}

static void test_log_syslog(void const *test_data)
{
        (void) test_data;

        TEST_LOG("syslog");
}

static void test_log_journal(void const *test_data)
{
        (void) test_data;

        TEST_LOG("journal");
}

static void test_log_null(void const *test_data)
{
        (void) test_data;

        TEST_LOG("null");
}

static void test_plugin_dir(void const *test_data)
{
        (void) test_data;

        static char *argv[] =
                { TEST_PROGRAM_NAME, "--plugin-dir", "/tmp/foo/bar" };

        RUN_CONFIG(argv);
}

static void test_path_manager(void const *test_data)
{
        (void) test_data;

        static char *argv[] =
                { TEST_PROGRAM_NAME, "--path-manager", "foo" };

        RUN_CONFIG(argv);
}

static void test_multi_arg(void const *test_data)
{
        (void) test_data;

        static char   plugin_dir[] = "/tmp/foo/bar";
        static char override_dir[] = "/tmp/foo/baz";
        assert(strcmp(plugin_dir, override_dir) != 0);

        static char          pm[] = "foo";
        static char override_pm[] = "bar";
        assert(strcmp(pm, override_pm) != 0);

        static char *argv[] = {
                TEST_PROGRAM_NAME,
                "--plugin-dir",   plugin_dir,
                "--path-manager", pm,
                "--plugin-dir",   override_dir,
                "--path-manager", override_pm
        };

        struct mptcpd_config *const config =
                mptcpd_config_create(L_ARRAY_SIZE(argv), argv);
        assert(config != NULL);

        assert(config->plugin_dir != NULL);
        assert(strcmp(config->plugin_dir, override_dir) == 0);

        assert(config->default_plugin != NULL);
        assert(strcmp(config->default_plugin, override_pm) == 0);

        mptcpd_config_destroy(config);
}


static void test_config_file(void const *test_data)
{
        (void) test_data;

        /**
         * @todo Test mptcpd configuration file parsing.  The
         *       configuration file location is currently compiled
         *       into the program since mptcpd may be run with
         *       elevated privileges.  A test can't be performed until
         *       mptcpd supports specifying the configuration file
         *       location at run-time, perhaps through a XDG-style
         *       approach (@c $XDG_CONFIG_DIRS and/or
         *       @c $XDG_CONFIG_HOME).
         */

        l_info("Configuration file test is unimplemented.");
}

int main(int argc, char *argv[])
{
        if (!l_main_init())
                return -1;

        l_log_set_stderr();

        l_test_init(&argc, &argv);

        l_test_add("log stderr",   test_log_stderr,   NULL);
        l_test_add("log syslog",   test_log_syslog,   NULL);
        l_test_add("log journal",  test_log_journal,  NULL);
        l_test_add("log null",     test_log_null,     NULL);
        l_test_add("plugin dir",   test_plugin_dir,   NULL);
        l_test_add("path manager", test_path_manager, NULL);
        l_test_add("multi arg",    test_multi_arg,    NULL);
        l_test_add("config file",  test_config_file,  NULL);
        l_test_add("debug",        test_debug,        NULL);

        l_test_run();

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
