// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpd.c
 *
 * @brief Main mptcpd source file.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>  // For NDEBUG
#endif

#include <stdlib.h>
#include <signal.h>
#include <assert.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>
#include <ell/main.h>
#pragma GCC diagnostic pop

#include <mptcpd/private/configuration.h>

#include "path_manager.h"


// Handle termination gracefully.
static void signal_handler(uint32_t signo, void *user_data)
{
        (void) user_data;

        assert(user_data != NULL);

        switch (signo) {
        case SIGINT:
        case SIGTERM:
                l_debug("\nTerminating %s", (char const *) user_data);
                l_main_quit();
                break;
        }
}

int main(int argc, char *argv[])
{
        int result = EXIT_SUCCESS;

        struct mptcpd_config *const config =
                mptcpd_config_create(argc, argv);

        if (config == NULL)
                return EXIT_FAILURE;

        if (!l_main_init()) {
                mptcpd_config_destroy(config);
                return EXIT_FAILURE;
        }

        // Initialize the path manager.
        struct mptcpd_pm *const pm = mptcpd_pm_create(config);

        if (pm == NULL) {
                result = EXIT_FAILURE;
                goto exit;
        }

        /**
         * @todo Start D-Bus once we support a mptcpd D-Bus API.
         *
         * @todo Make this daemon socket-activatable when using
         *       systemd.
         *
         * @todo Should we daemonize the the mptcpd process the
         *       canonical way - fork() then orphan to make it owned by
         *       the 'init' process, among other steps - when systemd
         *       isn't used?
         */

        // Start the main event loop.
        result = l_main_run_with_signal(signal_handler, argv[0]);

        if (result == EXIT_FAILURE)
                l_error("Main event loop failed.");

        mptcpd_pm_destroy(pm);

exit:
        /**
         * @todo Call @c mptcpd_config_free() as soon we're done with
         *       reading the configuration, e.g. after the path
         *       manager has been initialized.
         */
        mptcpd_config_destroy(config);

        if (!l_main_exit())
                result = EXIT_FAILURE;

        return result;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
