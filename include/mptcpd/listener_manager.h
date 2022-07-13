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
#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

struct l_hashmap;
struct sockaddr;

/**
 * @brief Create a MPTCP listener manager.
 *
 * @return Pointer to a MPTCP listener manager on success.  @c NULL on
 *         failure.
 */
MPTCPD_API struct l_hashmap *mptcpd_lm_create(void);

/**
 * @brief Destroy MPTCP listener manager.
 *
 * @param[in,out] lm The mptcpd address listener manager object.
 */
MPTCPD_API void mptcpd_lm_destroy(struct l_hashmap *lm);

/**
 * @brief Listen on the given MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] id The MPTCP local address ID.
 * @param[in] sa The MPTCP local address.
 *
 * @return @c true on success, and @c false on failure.
 */
MPTCPD_API bool mptcpd_lm_listen(struct l_hashmap *lm,
                                 mptcpd_aid_t id,
                                 struct sockaddr const *sa);

/**
 * @brief Stop listening on a MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] id The MPTCP local address ID.
 *
 * @return @c true on success, and @c false on failure.
 */
MPTCPD_API bool mptcpd_lm_close(struct l_hashmap *lm, mptcpd_aid_t id);

#ifdef __cplusplus
}
#endif


#endif /* MPTCPD_LISTENER_MANAGER_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
