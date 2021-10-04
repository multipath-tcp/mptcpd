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

#include <stdbool.h>

#include <mptcpd/private/netlink_pm.h>
#include <mptcpd/types.h>


struct mptcpd_pm;
struct sockaddr;

/// Get MPTCP generic netlink path manager characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void);

/**
 * @name User Space Path Management Netlink Commands
 *
 * The set of functions that implement client-oriented MPTCP path
 * management generic netlink command calls where path management is
 * performed in the user space. These functions are common to both the
 * upstream and multipath-tcp.org kernels.
 *
 * @todo TL;DR: The upstream and multipath-tcp.org kernel user space
 *       path management generic netlink APIs are source compatible
 *       but not binary compatible.  Work around that by passing the
 *       command identifier as an argument.
 *       @par
 *       All of these functions accept a @a cmd generic netlink
 *       command identifier argument since mptcpd currently supports
 *       both the upstream and multipath-tcp.org kernels at
 *       compile-time.  While both kernels support the same user space
 *       path management generic netlink API, the actual generic
 *       netlink commands values may differ.  For example, the
 *       enumerator for the @c MPTCP_CMD_ANNOUNCE command has a value
 *       of 4 but the generic netlink command with the same value in
 *       the upstream kernel is @c MPTCP_PM_CMD_FLUSH_ADDRS, which
 *       forces @c MPTCP_CMD_ANNOUNCE in the upstream kernel to have a
 *       different value.  The @a cmd argument could be dropped
 *       entirely if only one kernel is supported at compile time by
 *       mptcpd since we could then directly use the command symbolic
 *       names rather than their values.
 */
//@{
/**
 * @brief Advertise new network address to peers.
 *
 * @param[in] cmd   Kernel-specific @c MPTCP_CMD_ANNOUNCE generic
 *                  netlink command identifier.
 * @param[in] pm    The mptcpd path manager object.
 * @param[in] addr  Local IP address and port to be advertised through
 *                  the MPTCP protocol @c ADD_ADDR option.  The port
 *                  is optional, and is ignored if it is zero.
 * @param[in] id    MPTCP local address ID.
 * @param[in] token MPTCP connection token.
 *
 * @return @c 0 if operation was successful. -1 or @c errno
 *         otherwise.
 */
int netlink_pm_add_addr(uint8_t cmd,
                        struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t id,
                        mptcpd_token_t token);

/**
 * @brief Stop advertising network address to peers.
 *
 * @param[in] cmd   Kernel-specific @c MPTCP_CMD_REMOVE generic
 *                  netlink command identifier.
 * @param[in] pm    The mptcpd path manager object.
 * @param[in] id    MPTCP local address ID.
 * @param[in] token MPTCP connection token.
 *
 * @return @c 0 if operation was successful. -1 or @c errno
 *         otherwise.
 */
int netlink_pm_remove_addr(uint8_t cmd,
                           struct mptcpd_pm *pm,
                           mptcpd_aid_t id,
                           mptcpd_token_t token);

/**
 * @brief Create a new subflow.
 *
 * @param[in] cmd               Kernel-specific
 *                              @c MPTCP_CMD_SUB_CREATE generic
 *                              netlink command identifier.
 * @param[in] pm                The mptcpd path manager object.
 * @param[in] token             MPTCP connection token.
 * @param[in] local_address_id  MPTCP local address ID.
 * @param[in] remote_address_id MPTCP remote address ID.
 * @param[in] local_addr        MPTCP subflow local address
 *                              information, including the port.
 * @param[in] remote_addr       MPTCP subflow remote address
 *                              information, including the port.
 * @param[in] backup            Whether or not to set the MPTCP
 *                              subflow backup priority flag.
 *
 * @return @c 0 if operation was successful. -1 or @c errno
 *         otherwise.
 *
 * @todo There far too many parameters.  Reduce.
 */
int netlink_pm_add_subflow(uint8_t cmd,
                           struct mptcpd_pm *pm,
                           mptcpd_token_t token,
                           mptcpd_aid_t local_address_id,
                           mptcpd_aid_t remote_address_id,
                           struct sockaddr const *local_addr,
                           struct sockaddr const *remote_addr,
                           bool backup);

/**
 * @brief Remove a subflow.
 *
 * @param[in] cmd         Kernel-specific @c MPTCP_CMD_SUB_DESTROY
 *                        generic netlink command identifier.
 * @param[in] pm          The mptcpd path manager object.
 * @param[in] token       MPTCP connection token.
 * @param[in] local_addr  MPTCP subflow local address information,
 *                        including the port.
 * @param[in] remote_addr MPTCP subflow remote address information,
 *                        including the port.
 *
 * @return @c 0 if operation was successful. @c errno
 *         otherwise.
 */
int netlink_pm_remove_subflow(uint8_t cmd,
                              struct mptcpd_pm *pm,
                              mptcpd_token_t token,
                              struct sockaddr const *local_addr,
                              struct sockaddr const *remote_addr);

/**
 * @brief Set priority of a subflow.
 *
 * @param[in] cmd         Kernel-specific @c MPTCP_CMD_SUB_PRIORITY
 *                        generic netlink command identifier.
 * @param[in] pm          The mptcpd path manager object.
 * @param[in] token       MPTCP connection token.
 * @param[in] local_addr  MPTCP subflow local address information,
 *                        including the port.
 * @param[in] remote_addr MPTCP subflow remote address information,
 *                        including the port.
 * @param[in] backup      Whether or not to set the MPTCP subflow
 *                        backup priority flag.
 *
 * @return @c 0 if operation was successful. @c errno
 *         otherwise.
 */
int netlink_pm_set_backup(uint8_t cmd,
                          struct mptcpd_pm *pm,
                          mptcpd_token_t token,
                          struct sockaddr const *local_addr,
                          struct sockaddr const *remote_addr,
                          bool backup);
//@}


#endif /* MPTCPD_NETLINK_PM_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
