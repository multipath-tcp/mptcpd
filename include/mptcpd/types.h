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

/// MPTCP connection token format specifier.
#define MPTCPD_PRIxTOKEN "#010" PRIx32

/// MPTCP address ID format specifier.
#define MPTCPD_PRIxAID PRIx8

/**
 * @brief Mask leading MPTCP connection token bits.
 *
 * Logging the MPTCP connection token is a security risk.  Mask off
 * the leading bits to make logging the @a token safer.
 *
 * @param[in] token MPTCP connection token.
 *
 * @return MPTCP connection stripped of leading bits.
 */
static inline mptcpd_token_t mptcpd_masked_token(mptcpd_token_t token)
{
        static mptcpd_token_t const mask = 0xFF;

        return token & mask;
}

#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_TYPES_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
