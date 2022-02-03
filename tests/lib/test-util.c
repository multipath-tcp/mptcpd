// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-util.c
 *
 * @brief mptcpd test utilities library.
 *
 * Copyright (c) 2020-2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <unistd.h>
#include <stdlib.h>

#if defined(HAVE_LINUX_MPTCP_H_UPSTREAM) \
        || defined(HAVE_LINUX_MPTCP_H_MPTCP_ORG)
# include <linux/mptcp.h>
#elif defined(HAVE_UPSTREAM_KERNEL)
# include <mptcpd/private/mptcp_upstream.h>
#else
# include <mptcpd/private/mptcp_org.h>
#endif

#include "../src/netlink_pm.h"  // For MPTCP_SYSCTL_BASE

#include "test-util.h"


char const *tests_get_pm_family_name(void)
{
#ifdef MPTCP_PM_NAME
        return MPTCP_PM_NAME;
#else
        return MPTCP_GENL_NAME;
#endif
}

static bool is_mptcp_kernel(void)
{
        // Kernel supports MPTCP if /proc/sys/net/mptcp exists.
        return access(MPTCP_SYSCTL_BASE, R_OK | X_OK) == 0;
}

void tests_skip_if_no_mptcp(void)
{
        /*
          An exit status of 77 causes the Automake test driver,
          i.e. the `test-driver' script, to consider the test result
          as SKIP rather than PASS or FAIL.

          Tests that should be skipped under some conditions, such as
          when not running a MPTCP capable kernel, should exit the
          process with this value.
        */
        static int const SKIP_EXIT_STATUS = 77;

        if (!is_mptcp_kernel())
                exit(SKIP_EXIT_STATUS);
}



/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
