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
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] sa The MPTCP local address.
 *
 * @return Non-zero port in network byte order to which the listener
 *         was bound, and zero on failure.  An ephemeral port will be
 *         returned if the port in the local address @a sa is zero.
 *         The @c sockaddr passed to subsequent calls to
 *         @c mptcpd_lm_close() should take this into account since
 *         the mptcpd listener manager will only keep track of
 *         addresses with non-zero ports, including addresses with
 *         ephemeral ports.
 */
MPTCPD_API in_port_t mptcpd_lm_listen(struct mptcpd_lm *lm,
                                      struct sockaddr const *sa);

/**
 * @brief Stop listening on a MPTCP local address.
 *
 * @param[in] lm The mptcpd address listener manager object.
 * @param[in] sa The MPTCP local address with the non-zero port
 *               returned from @c mptcpd_lm_listen(), i.e. the
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
