// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/id_manager.h
 *
 * @brief Map of MPTCP address ID to network address - private API.
 *
 * Copyright (c) 2020, 2021, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_ID_MANAGER_H
#define MPTCPD_PRIVATE_ID_MANAGER_H

#include <stdbool.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

struct mptcpd_idm;
struct sockaddr;

/**
 * @brief Map an IP address to a MPTCP address ID.
 *
 * Map an IP address to a MPTCP address ID.  The MPTCP addresses ID
 * for an existing IP address will be updated with the new ID.
 *
 * @note This function is only meant for internal use by mptcpd.
 *
 * @param[in] idm The mptcpd address ID manager object.
 * @param[in] sa  IP address information.
 * @param[in] id  MPTCP address ID.
 *
 * @return @c true if mapping succeeded, and @c false otherwise.
 */
MPTCPD_API bool mptcpd_idm_map_id(struct mptcpd_idm *idm,
                                  struct sockaddr const *sa,
                                  mptcpd_aid_t id);


#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PRIVATE_ID_MANAGER_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
