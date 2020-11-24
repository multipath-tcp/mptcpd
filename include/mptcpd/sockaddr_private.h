// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file sockaddr_private.h
 *
 * @brief mptcpd @c struct @c sockaddr related utility functions.
 *
 * Copyright (c) 2019-2020, Intel Corporation
 */

#ifndef MPTCPD_SOCKADDR_PRIVATE_H
#define MPTCPD_SOCKADDR_PRIVATE_H

#include <stdbool.h>

#include <mptcpd/export.h>


#ifdef __cplusplus
extern "C" {
#endif

struct in_addr;
struct in6_addr;
struct sockaddr_storage;

/**
 * @brief Initialize @c sockaddr_storage instance.
 *
 * Initialize a @c sockaddr_storage instance with the provided IPv4 or
 * IPv6 address.  Only one is required and used. The @a port may be
 * zero in cases where it is optional.
 *
 * @param[in]     addr4 IPv4 internet address.
 * @param[in]     addr6 IPv6 internet address.
 * @param[in]     port  IP port.
 * @param[in,out] addr  mptcpd network address information.
 *
 * @return @c true on success.  @c false otherwise.
 */
MPTCPD_API bool
mptcpd_sockaddr_storage_init(struct in_addr  const *addr4,
                             struct in6_addr const *addr6,
                             unsigned short port,
                             struct sockaddr_storage *addr);

#ifdef __cplusplus
}
#endif


#endif  /* MPTCPD_SOCKADDR_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
