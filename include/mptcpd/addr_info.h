// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file addr_info.h
 *
 * @brief @c mptcpd_addr_info related functions.
 *
 * Copyright (c) 2020-2021, Intel Corporation
 */

#ifndef MPTCPD_ADDR_INFO_H
#define MPTCPD_ADDR_INFO_H

#include <mptcpd/export.h>
#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_addr_info;

/**
 * @brief Get underlying network address related information.
 *
 * @param[in] info Mptcpd address information.
 *
 * @return @c sockaddr object containing the underlying network
 *         address information.  Cast to a @c const pointer to a
 *         @c sockaddr_in or @c sockaddr_in6 depending on the address
 *         family in the @c sa_family field.  @c NULL is returned on
 *         error.
 */
MPTCPD_API struct sockaddr const *
mptcpd_addr_info_get_addr(struct mptcpd_addr_info const *info);

/**
 * @brief Get MPTCP address ID.
 *
 * @param[in] info Mptcpd address information.
 *
 * @return MPTCP address ID corresponding to network address
 *         encapsulated by @a info.  0 is returned on error.
 */
MPTCPD_API mptcpd_aid_t
mptcpd_addr_info_get_id(struct mptcpd_addr_info const *info);

/**
 * @brief Get mptcpd flags associated with a network address.
 *
 * @param[in] info Mptcpd address information.
 *
 * @return Mptcpd flags bitmask associated with the network address
 *         encapsulated by @a info.  0 is returned on error.
 */
MPTCPD_API mptcpd_flags_t
mptcpd_addr_info_get_flags(struct mptcpd_addr_info const *info);

/**
 * @brief Get network interface index associated with an address.
 *
 * @param[in] info Mptcpd address information.
 *
 * @return Network interface index associated with the network address
 *         encapsulated by @a info.  -1 is returned on error.
 */
MPTCPD_API int
mptcpd_addr_info_get_index(struct mptcpd_addr_info const *info);

#ifdef __cplusplus
}
#endif


#endif  /* MPTCPD_ADDR_INFO_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
