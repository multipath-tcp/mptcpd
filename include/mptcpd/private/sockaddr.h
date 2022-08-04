// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/sockaddr.h
 *
 * @brief mptcpd @c struct @c sockaddr related utility functions.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_SOCKADDR_H
#define MPTCPD_PRIVATE_SOCKADDR_H

#include <stdbool.h>
#include <endian.h>
#include <byteswap.h>

#include <netinet/in.h>   // For in_addr_t.

#include <mptcpd/export.h>

/**
 * @name Swap host ordered bytes in an integer to network byte order.
 *
 * These macros may be used in place of @c htons() or @c htonl() when
 * initializing an IPv4 address or IP port constant at compile-time.
 */
///@{
#if __BYTE_ORDER == __LITTLE_ENDIAN
# define MPTCPD_CONSTANT_HTONS(hs) __bswap_constant_16(hs)
# define MPTCPD_CONSTANT_HTONL(hl) __bswap_constant_32(hl)
#else
// No need to swap bytes on big endian platforms.
// host byte order == network byte order
# define MPTCPD_CONSTANT_HTONS(hs) hs
# define MPTCPD_CONSTANT_HTONL(hl) hl
#endif  // __BYTE_ORDER == __LITTLE_ENDIAN
///@}


#ifdef __cplusplus
extern "C" {
#endif

struct in6_addr;
struct sockaddr_storage;

/**
 * @brief Initialize @c sockaddr_storage instance.
 *
 * Initialize a @c sockaddr_storage instance with the provided IPv4 or
 * IPv6 address.  Only one is required and used. The @a port may be
 * zero in cases where it is optional.
 *
 * @param[in]     addr4 IPv4 internet address (network byte order).
 * @param[in]     addr6 IPv6 internet address.
 * @param[in]     port  IP port (network byte order).
 * @param[in,out] addr  mptcpd network address information.
 *
 * @return @c true on success.  @c false otherwise.
 */
MPTCPD_API bool
mptcpd_sockaddr_storage_init(in_addr_t const *addr4,
                             struct in6_addr const *addr6,
                             in_port_t port,
                             struct sockaddr_storage *addr);

/**
 * @brief Deep copy a @c sockaddr.
 *
 * Copy the address family-specific contents of a @c sockaddr.  For an
 * @c AF_INET address family, a @c struct @c sockaddr_in will be
 * dynamically allocated and copied from @a sa.  Similarly, @c struct
 * @c sockaddr_in6 will be allocated and copied from @a sa for the
 * @c AF_INET6 address family case.
 *
 * @return Dynamically allocated copy of @a sa if the @c sa_family
 *         member is @c AF_INET or @c AF_INET6, and @c NULL otherwise.
 *         Deallocate with @c l_free().
 */
MPTCPD_API struct sockaddr *
mptcpd_sockaddr_copy(struct sockaddr const *sa);

#ifdef __cplusplus
}
#endif


#endif  /* MPTCPD_PRIVATE_SOCKADDR_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
