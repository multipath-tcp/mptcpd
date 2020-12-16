// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpd/path_manager.h
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifndef MPTCPD_LIB_PATH_MANAGER_H
#define MPTCPD_LIB_PATH_MANAGER_H

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_pm;
struct mptcpd_addr_info;
struct mptcpd_limits;

/**
 * @brief Is mptcpd path manager ready for use?
 *
 * The mptcpd path manager is ready for use when the @c "mptcp"
 * generic netlink family is available in the Linux kernel.  No
 * path management related interaction with the kernel can occur until
 * that family appears.
 *
 * @param[in] pm Mptcpd path manager.
 *
 * @return @c true if the mptcpd path manager is ready for use, and @c
 *         false otherwise.
 */
MPTCPD_API bool mptcpd_pm_ready(struct mptcpd_pm const *pm);

/**
 * @brief Advertise new network address to peers.
 *
 * @param[in] pm    The mptcpd path manager object.
 * @param[in] addr  Local IP address and port to be advertised
 *                  through the MPTCP protocol @c ADD_ADDR
 *                  option.  The port is optional, and is
 *                  ignored if it is zero.
 * @param[in] id    MPTCP local address ID.
 * @param[in] flags Bitset of MPTCP flags associated with the network
 *                  address, e.g. @c MPTCP_ADDR_FLAG_BACKUP @c |
 *                   @c MPTCP_PM_ADDR_FLAG_SUBFLOW.  Optional for
 *                  upstream kernel.  Unused by the multipath-tcp.org
 *                  Linux kernel (e.g. set to zero).
 * @param[in] index Network interface index.  Optional for upstream
 *                  Linux kernel (e.g. set to zero).
 * @param[in] token MPTCP connection token.  Unused by the upstream
 *                  Linux kernel (e.g. set to zero).
 *
 * @todo Should we define corresponding mptcpd flags instead of
 *       exposing the flags in <linux/mptcp.h>?
 *
 * @return @c 0 if operation was successful. @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_add_addr(struct mptcpd_pm *pm,
                                  struct sockaddr const *addr,
                                  mptcpd_aid_t id,
                                  uint32_t flags,
                                  int index,
                                  mptcpd_token_t token);

/**
 * @brief Stop advertising network address to peers.
 *
 * @param[in] pm         The mptcpd path manager object.
 * @param[in] address_id MPTCP local address ID to be sent in the
 *                       MPTCP protocol @c REMOVE_ADDR option
 *                       corresponding to the local address that will
 *                       no longer be available.
 * @param[in] token      MPTCP connection token.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_remove_addr(struct mptcpd_pm *pm,
                                     mptcpd_aid_t address_id,
                                     mptcpd_token_t token);

/**
 * @brief Get network address corresponding to an address ID.
 *
 * @param[in] pm       The mptcpd path manager object.
 * @param[in] id       MPTCP local address ID.
 * @param[in] callback Function to be called when the network address
 *                     corresponding to the given MPTCP address @a id
 *                     has been retrieved.
 * @param[in] data     Data to be passed to the @a callback function.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_get_addr(struct mptcpd_pm *pm,
                                  mptcpd_aid_t id,
                                  mptcpd_pm_get_addr_cb callback,
                                  void *data);

/**
 * @brief Get list (array) of MPTCP network addresses.
 *
 * @param[in] pm       The mptcpd path manager object.
 * @param[in] callback Function to be called when a dump of network
 *                     addresses has been retrieved.
 * @param[in] data     Data to be passed to the @a callback function.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_dump_addrs(struct mptcpd_pm *pm,
                                    mptcpd_pm_dump_addrs_cb callback,
                                    void *data);

/**
 * @brief Flush MPTCP addresses.
 *
 * @param[in] pm The mptcpd path manager object.
 *
 * @todo Improve documentation.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_flush_addrs(struct mptcpd_pm *pm);

/**
 * @brief Set MPTCP resource limits.
 *
 * @param[in] pm     The mptcpd path manager object.
 * @param[in] limits Array of MPTCP resource type/limit pairs.
 * @param[in] len    Length of the @a limits array.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_set_limits(struct mptcpd_pm *pm,
                                    struct mptcpd_limit const *limits,
                                    size_t len);

/**
 * @brief Get MPTCP resource limits.
 *
 * @param[in] pm       The mptcpd path manager object.
 * @param[in] callback Function to be called when the MPTCP resource
 *                     limits have been retrieved.
 * @param[in] data     Data to be passed to the @a callback function.
 *
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_get_limits(struct mptcpd_pm *pm,
                                    mptcpd_pm_get_limits_cb callback,
                                    void *data);

/**
 * @brief Create a new subflow.
 *
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
 * @return @c 0 if operation was successful. -1 or @c errno otherwise.
 *
 * @todo There far too many parameters.  Reduce.
 */
MPTCPD_API int
mptcpd_pm_add_subflow(struct mptcpd_pm *pm,
                      mptcpd_token_t token,
                      mptcpd_aid_t local_address_id,
                      mptcpd_aid_t remote_address_id,
                      struct sockaddr const *local_addr,
                      struct sockaddr const *remote_addr,
                      bool backup);

/**
 * @brief Set priority of a subflow.
 *
 * @param[in] pm                The mptcpd path manager object.
 * @param[in] token             MPTCP connection token.
 * @param[in] local_addr        MPTCP subflow local address
 *                              information, including the port.
 * @param[in] remote_addr       MPTCP subflow remote address
 *                              information, including the port.
 * @param[in] backup            Whether or not to set the MPTCP
 *                              subflow backup priority flag.
 *
 * @return @c 0 if operation was successful. @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_set_backup(
        struct mptcpd_pm *pm,
        mptcpd_token_t token,
        struct sockaddr const *local_addr,
        struct sockaddr const *remote_addr,
        bool backup);

/**
 * @brief Remove a subflow.
 *
 * @param[in] pm                The mptcpd path manager object.
 * @param[in] token             MPTCP connection token.
 * @param[in] local_addr        MPTCP subflow local address
 *                              information, including the port.
 * @param[in] remote_addr       MPTCP subflow remote address
 *                              information, including the port.
 *
 * @return @c 0 if operation was successful. @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_remove_subflow(
        struct mptcpd_pm *pm,
        mptcpd_token_t token,
        struct sockaddr const *local_addr,
        struct sockaddr const *remote_addr);

/**
 * @brief Get pointer to the underlying network monitor.
 *
 * @param[in] pm Mptcpd path manager.
 *
 * @return Mptcpd network monitor.
 */
MPTCPD_API struct mptcpd_nm const *
mptcpd_pm_get_nm(struct mptcpd_pm const *pm);

/**
 * @brief Get pointer to the underlying MPTCP address ID manager.
 *
 * @param[in] pm Mptcpd path manager.
 *
 * @return Mptcpd MPTCP address ID manager.
 */
MPTCPD_API struct mptcpd_idm *
mptcpd_pm_get_idm(struct mptcpd_pm const *pm);

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_LIB_PATH_MANAGER_H

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
