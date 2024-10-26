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

#include <ell/ell.h>

#include "netlink_pm.h"


bool mptcpd_is_kernel_mptcp_enabled(char const *path,
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

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
