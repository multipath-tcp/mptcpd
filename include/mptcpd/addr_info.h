// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file addr_info.h
 *
 * @brief @c mptcpd_addr_info related code.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#ifndef MPTCPD_ADDR_INFO_H
#define MPTCPD_ADDR_INFO_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @struct mptcpd_addr_info addr_info.h <mptcpd/addr_info.h>
 *
 * @brief Information associated with a network address.
 */
struct mptcpd_addr_info
{
        /**
         * @brief Network address family and IP address.
         *
         * @note Cast to @c sockaddr_in or @c sockaddr_in6 depending
         *       on the address family in the @c ss_family field.
         */
        struct sockaddr_storage addr;

        /**
         * MPTCP address ID associated with the network address.
         *
         * @note This value will be zero if no ID is associated with
         *       the address.
         */
        mptcpd_aid_t id;

        /**
         * @brief MPTCP flags associated with the network address.
         *
         * Bitset of MPTCP flags associated with the network address,
         * e.g. @c MPTCPD_ADDR_FLAG_BACKUP @c |
         * @c MPTCP_PM_ADDR_FLAG_SUBFLOW.
         *
         * @note This value will be zero if no flag bits are set.
         */
        uint32_t flags;

        /**
         * @brief Network interface index associated with network address.
         *
         * @todo Will this value ever be zero, i.e. not set?
         */
        int index;
};

#ifdef __cplusplus
}
#endif


#endif  /* MPTCPD_ADDR_INFO_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
