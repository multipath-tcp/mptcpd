// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/netlink_pm.h
 *
 * @brief Kernel generic netlink path manager details.
 *
 * Copyright (c) 2021, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_NETLINK_PM_H
#define MPTCPD_PRIVATE_NETLINK_PM_H


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct mptcpd_netlink_pm
 *
 * Kernel-specific MPTCP generic netlink path manager
 * characteristics.
 */
struct mptcpd_netlink_pm
{
        /// MPTCP generic netlink family name.
        char const *const name;

        /// MPTCP generic netlink multicast group.
        char const *const group;

        /// MPTCP path management generic netlink command functions.
        struct mptcpd_pm_cmd_ops const *const cmd_ops;
};


#endif /* MPTCPD_PRIVATE_NETLINK_PM_H */

#ifdef __cplusplus
}
#endif


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
