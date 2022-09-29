// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/network_monitor.h
 *
 * @brief mptcpd network device monitoring - internal API.
 *
 * Copyright (c) 2017-2022, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_NETWORK_MONITOR_H
#define MPTCPD_PRIVATE_NETWORK_MONITOR_H

#include <mptcpd/export.h>

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mptcpd_nm;

/**
 * @name Mptcpd Network Monitor Flags
 *
 * Flags controlling address notification in the mptcpd network
 * monitor.  Pass to @c mptcpd_nm_create().
 */
///@{
/// Notify even the addresses already existing at startup-time.
#define MPTCPD_NOTIFY_FLAG_EXISTING (1U << 0)

/// Ignore link-local addresses.
#define MPTCPD_NOTIFY_FLAG_SKIP_LL (1U << 1)

/// Ignore host (loopback) addresses.
#define MPTCPD_NOTIFY_FLAG_SKIP_HOST (1U << 2)

/**
 * Notify address only if a default route is available from the given
 * interface.
 */
#define MPTCPD_NOTIFY_FLAG_ROUTE_CHECK (1U << 3)
///@}

/**
 * @brief Create a network monitor.
 *
 * @param[in] flags Flags controlling address notification, any of:
 *                  MPTCPD_NOTIFY_FLAG_EXISTING,
 *                  MPTCPD_NOTIFY_FLAG_SKIP_LL,
 *                  MPTCPD_NOTIFY_FLAG_SKIP_HOST
 *
 * @todo As currently implemented, one could create multiple network
 *       monitors.  Is that useful?
 *
 * @return Pointer to new network monitor on success.  @c NULL on
 *         failure.
 */
MPTCPD_API struct mptcpd_nm *mptcpd_nm_create(uint32_t flags);

/**
 * @brief Destroy a network monitor.
 *
 * @param[in,out] nm Network monitor to be destroyed.
 */
MPTCPD_API void mptcpd_nm_destroy(struct mptcpd_nm *nm);

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PRIVATE_NETWORK_MONITOR_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
