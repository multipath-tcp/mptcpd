// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file listener_manager.h
 *
 * @brief Map of MPTCP local address ID to listener.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifndef MPTCPD_LISTENER_MANAGER_H
#define MPTCPD_LISTENER_MANAGER_H

#include <stdbool.h>

#include <mptcpd/export.h>


#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_lm;

/**
 * @brief Create a MPTCP listener manager.
 *
 * @return Pointer to a MPTCP listener manager on success.  @c NULL on
 *         failure.
 */
MPTCPD_API struct mptcpd_lm *mptcpd_lm_create(void);

/**
 * @brief Destroy MPTCP listener manager.
 *
 * @param[in,out] lm The mptcpd address listener manager object.
 */
MPTCPD_API void mptcpd_lm_destroy(struct mptcpd_lm *lm);

/**
 * @brief Listen on the given MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] sa The MPTCP local address.
 *
 * @return @c true on success, and @c false on failure.
 */
MPTCPD_API bool mptcpd_lm_listen(struct mptcpd_lm *lm,
                                 struct sockaddr const *sa);

/**
 * @brief Stop listening on a MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] sa The MPTCP local IP address.
 *
 * @return @c true on success, and @c false on failure.
 */
MPTCPD_API bool mptcpd_lm_close(struct mptcpd_lm *lm,
                                struct sockaddr const *sa);

#ifdef __cplusplus
}
#endif


#endif /* MPTCPD_LISTENER_MANAGER_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
