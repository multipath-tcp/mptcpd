// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file listener_manager.h
 *
 * @brief Map of MPTCP local address to listener.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifndef MPTCPD_LISTENER_MANAGER_H
#define MPTCPD_LISTENER_MANAGER_H

#include <netinet/in.h>

#include <mptcpd/export.h>


#ifdef __cplusplus
extern "C" {
#endif

struct mptcpd_lm;

/**
 * @brief Listen on the given MPTCP local address.
 *
 * Create a MPTCP listening socket for the given local address.  This
 * is needed to accept subflows, e.g. during a @c MP_JOIN operation.
 *
 * @param[in]     lm The mptcpd address listener manager object.
 * @param[in,out] sa The MPTCP local address.  If the port is zero an
 *                   ephemeral port will be chosen, and assigned to
 *                   the appropriate underlying address
 *                   family-specific port member, e.g. @c sin_port or
 *                   @c sin6_port.  The port will be in network byte
 *                   order.
 *
 * @return @c true on success, and @c false on failure.
 */
MPTCPD_API bool mptcpd_lm_listen(struct mptcpd_lm *lm,
                                 struct sockaddr *sa);

/**
 * @brief Stop listening on a MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] sa The MPTCP local address with a non-zero port, such as
 *               the one assigned by @c mptcpd_lm_listen(), i.e. the
 *               non-zero port provided by the user or the ephemeral
 *               port chosen by the kernel.
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
