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
 * @param[in] flags 
 * @param[in] index Network interface index (optional for
 *                  upstream Linux kernel).
 * @param[in] token MPTCP connection token.
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
 * @param[in] flags      
 * @param[in] token      MPTCP connection token.
 *
 * @return @c 0 if operation was successful. @c errno otherwise.
 */
MPTCPD_API int mptcpd_pm_remove_addr(struct mptcpd_pm *pm,
                                     mptcpd_aid_t address_id,
                                     uint32_t flags,
                                     mptcpd_token_t token);

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
 * @return @c 0 if operation was successful. @c errno otherwise.
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

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_LIB_PATH_MANAGER_H

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
