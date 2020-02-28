// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file network_monitor_private.h
 *
 * @brief mptcpd network monitor private interface.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#ifndef MPTCPD_NETWORK_MONITOR_PRIVATE_H
#define MPTCPD_NETWORK_MONITOR_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

struct mptcpd_interface;
struct sockaddr;

/**
 * @struct mptcpd_addr_info network_monitor_private.h <mptcpd/network_monitor_private.h>
 *
 * @brief Convenience structure to bundle address information.
 */
struct mptcpd_addr_info
{
        /// Network interface information.
        struct mptcpd_interface const *const interface;

        /// Network address information.
        struct sockaddr const *const address;
};

#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_NETWORK_MONITOR_PRIVATE_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
