// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/listener_manager.h
 *
 * @brief Map of MPTCP local address to listener - private API.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_LISTENER_MANAGER_H
#define MPTCPD_PRIVATE_LISTENER_MANAGER_H

#include <mptcpd/export.h>


#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif


#endif /* MPTCPD_PRIVATE_LISTENER_MANAGER_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
