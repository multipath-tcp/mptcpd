// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file types.h
 *
 * @brief mptcpd user space path manager attribute types.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#ifndef MPTCPD_TYPES_H
#define MPTCPD_TYPES_H

#include <stdint.h>
#include <inttypes.h>
#include <netinet/in.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @todo These rely on MPTCP genl related implementation details in
 *        the kernel. Should we move these typedefs to
 *        <linux/mptcp_genl.h>, e.g. 'typedef uint32_t mptcp_cid_t'?
 */
/// MPTCP connection ID type.
typedef uint32_t mptcpd_cid_t;

/// MPTCP address ID type.
typedef uint8_t mptcpd_aid_t;

/// MPTCP connection ID format specifier.
#define MPTCPD_PRIxCID PRIx32

/// MPTCP address ID format specifier.
#define MPTCPD_PRIxAID PRIx8


/**
 * @struct mptcpd_in_addr types.h <mptcpd/types.h>
 *
 * @brief MPTCP/TCP address information.
 */
struct mptcpd_in_addr
{
        /// Address family: @c AF_INET or @c AF_INET6
        sa_family_t family;

        /**
         * @union addr types.h <mptcpd/types.h>
         *
         * @brief Internet address.
         *
         * Use @c addr4 member if the @c family is @c AF_INET, and
         * @c addr6 otherwise.
         */
        union addr
        {
                /**
                 * @brief The IPv4 address.
                 *
                 * @note Valid when @c family is @c AF_INET.
                 */
                struct in_addr addr4;

                /**
                 * @brief The IPv6 address.
                 *
                 * @note Valid when @c family is @c AF_INET6.
                 */
                struct in6_addr addr6;
        } addr; /**< @brief Internet address. */
};

/**
 * @struct mptcpd_addr types.h <mptcpd/types.h>
 *
 * @brief MPTCP connection/subflow address information.
 *
 * @todo There no longer appears to be a real need for this
 *       mptcpd-specific type since the original motivation for it
 *       (avoid deep copying addrs) no longer exists.  We should
 *       probably switch to the standard @c sockaddr_in,
 *       @c sockaddr_in6 types.
 */
struct mptcpd_addr
{
        /// Internet address.
        struct mptcpd_in_addr address;

        /// Port number.
        in_port_t port;
};

#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_TYPES_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
