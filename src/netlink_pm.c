// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file netlink_pm.c
 *
 * @brief Kernel generic netlink path manager details.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#include <stdbool.h>
#include <stdio.h>

#include <ell/log.h>
#include <ell/util.h>

#include "netlink_pm.h"

/// Directory containing MPTCP sysctl variable entries.
#define MPTCP_SYSCTL_BASE "/proc/sys/net/mptcp/"

/// Constuct full path for MPTCP sysctl variable "@a name".
#define MPTCP_SYSCTL_VARIABLE(name) MPTCP_SYSCTL_BASE #name

/// Get multipath-tcp.org kernel generic netlink PM characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm_mptcp_org(void);

/// Get upstream kernel generic netlink PM characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm_upstream(void);

/**
 * @brief Verify that MPTCP is enabled at run-time in the kernel.
 *
 * Check that MPTCP is enabled through the specified @c sysctl
 * variable.
 */
static bool check_kernel_mptcp_enabled(char const *path,
                                       char const *variable,
                                       int enable_val)
{
        FILE *const f = fopen(path, "r");

        if (f == NULL)
                return false;  // Not using kernel that supports given
                               // MPTCP sysctl variable.

        int enabled = 0;
        int const n = fscanf(f, "%d", &enabled);

        fclose(f);

        if (n == 1) {
                if (enabled == 0) {
                        l_error("MPTCP is not enabled in the kernel.");
                        l_error("Try 'sysctl -w net.mptcp.%s=%d'.",
                                variable,
                                enable_val);

                        /*
                          Mptcpd should not set this variable since it
                          would need to be run as root in order to
                          gain write access to files in
                          /proc/sys/net/mptcp/ owned by root.
                          Ideally, mptcpd should not be run as the
                          root user.
                        */
                }
        } else {
                l_error("Unable to determine if MPTCP is enabled.");
        }

        return enabled;
}

/// Are we being run under a MPTCP-capable upstream kernel?
static bool is_upstream_kernel(void)
{
        static char const path[] = MPTCP_SYSCTL_VARIABLE(enabled);
        static char const name[] = "enabled";
        static int  const enable_val = 1;

        return check_kernel_mptcp_enabled(path, name, enable_val);
}

/// Are we being run under a MPTCP-capable multipath-tcp.org kernel?
static bool is_mptcp_org_kernel(void)
{
        static char const path[] = MPTCP_SYSCTL_VARIABLE(mptcp_enabled);
        static char const name[] = "mptcp_enabled";
        static int  const enable_val = 2;  // or 1

        return check_kernel_mptcp_enabled(path, name, enable_val);
}

struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void)
{
        if (is_upstream_kernel()) {
                return mptcpd_get_netlink_pm_upstream();
        } else if (is_mptcp_org_kernel()) {
                return mptcpd_get_netlink_pm_mptcp_org();
        } else {
                return NULL;
        }
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
