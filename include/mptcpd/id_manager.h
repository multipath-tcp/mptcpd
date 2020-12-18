// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file id_manager.h
 *
 * @brief Map of network address to MPTCP address ID.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#ifndef MPTCPD_ID_MANAGER_H
#define MPTCPD_ID_MANAGER_H

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mptcpd_idm;
struct sockaddr;

/**
 * @brief Create MPTCP address ID manager.
 *
 * @return Pointer to MPTCP address ID manager on success.  @c NULL on
 *         failure.
 */
MPTCPD_API struct mptcpd_idm *mptcpd_idm_create(void);

/**
 * @brief Destroy MPTCP address ID manager.
 *
 * @param[in,out] idm The mptcpd address ID manager object.
 */
MPTCPD_API void mptcpd_idm_destroy(struct mptcpd_idm *idm);

/**
 * @brief Get MPTCP address ID.
 *
 * Map an IP address to a MPTCP address ID, and return that ID.
 *
 * @param[in] idm The mptcpd address ID manager object.
 * @param[in] sa  IP address information.
 *
 * @return MPTCP address ID associated with the address @a sa, or zero
 *         on error.
 */
MPTCPD_API mptcpd_aid_t mptcpd_idm_get_id(struct mptcpd_idm *idm,
                                          struct sockaddr const *sa);

/**
 * @brief Remove MPTCP address ID.
 *
 * Stop associating the MPTCP address ID with the given IP address.
 *
 * @param[in] idm The mptcpd address ID manager object.
 * @param[in] sa  IP address information.
 *
 * @return MPTCP address ID that was removed, or zero if no such ID is
 *         associated with the IP address @a sa.  Zero will also be
 *         returned if either of the arguments are NULL.
 */
MPTCPD_API mptcpd_aid_t mptcpd_idm_remove_id(struct mptcpd_idm *idm,
                                             struct sockaddr const *sa);

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_ID_MANAGER_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
