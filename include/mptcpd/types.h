// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file types.h
 *
 * @brief mptcpd user space path manager attribute types.
 *
 * Copyright (c) 2018-2020, Intel Corporation
 */

#ifndef MPTCPD_TYPES_H
#define MPTCPD_TYPES_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @todo These rely on MPTCP genl related implementation details in
 *        the kernel. Should we move these typedefs to
 *        @c <linux/mptcp.h>, e.g. 'typedef uint32_t mptcp_token_t'?
 */
/// MPTCP connection token type.
typedef uint32_t mptcpd_token_t;

/// MPTCP address ID type.
typedef uint8_t mptcpd_aid_t;

/// MPTCP address ID format specifier.
#define MPTCPD_PRIxAID PRIx8

/// MPTCP flags type.
typedef uint32_t mptcpd_flags_t;

/// Maximum number of address advertisements to receive.
#define MPTCPD_LIMIT_RCV_ADD_ADDRS MPTCP_PM_ATTR_RCV_ADD_ADDRS

/// Maximum number of subflows.
#define MPTCPD_LIMIT_SUBFLOWS MPTCP_PM_ATTR_SUBFLOWS

/**
 * @struct mptcpd_limit
 *
 * @brief MPTCP resource type/limit pair.
 */
struct mptcpd_limit
{
        /// MPTCP resource type, e.g. @c MPTCPD_LIMIT_SUBFLOWS.
        uint16_t type;

        /// MPTCP resource limit value.
        uint32_t limit;
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
