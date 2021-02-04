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


/// Get MPTCP generic netlink path manager characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void);


#endif /* MPTCPD_NETLINK_PM_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
