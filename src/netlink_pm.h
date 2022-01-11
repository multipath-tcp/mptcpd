// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file src/netlink_pm.h
 *
 * @brief Kernel generic netlink path manager details.
 *
 * Copyright (c) 2021, Intel Corporation
 */

#ifndef MPTCPD_NETLINK_PM_H
#define MPTCPD_NETLINK_PM_H

#include <mptcpd/private/netlink_pm.h>

#include <mptcpd/types.h>

#include <stdbool.h>

/// Directory containing MPTCP sysctl variable entries.
#define MPTCP_SYSCTL_BASE "/proc/sys/net/mptcp/"

/// Constuct full path for MPTCP sysctl variable "@a name".
#define MPTCP_SYSCTL_VARIABLE(name) MPTCP_SYSCTL_BASE #name

struct sockaddr;
struct mptcpd_pm;

/// Get MPTCP generic netlink path manager characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void);

/**
 * @brief Verify that MPTCP is enabled at run-time in the kernel.
 *
 * Check that MPTCP is enabled through the specified @c sysctl
 * variable.
 *
 * @param[in] path       Path to file containing MPTCP sysctl "enable"
 *                       value, e.g. "/proc/sys/net/mptcp/enabled".
 * @param[in] variable   Name of "enabled" variable.
 * @param[in] enable_val Value needed to enable MPTCP in the kernel.
 *
 * @return @c true if MPTCP is enabled in the kernel, and @c false
 *         otherwise.
 */
bool mptcpd_is_kernel_mptcp_enabled(char const *path,
                                    char const *variable,
                                    int enable_val);


#endif /* MPTCPD_NETLINK_PM_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
