// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file types.h
 *
 * @brief mptcpd user space path manager attribute types.
 *
 * Copyright (c) 2018-2021, Intel Corporation
 */

#ifndef MPTCPD_TYPES_H
#define MPTCPD_TYPES_H

#include <stddef.h>
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

/**
 * @name MPTCP Address Flags
 *
 * Each MPTCP address flag is meant to be set as a bit in a
 * @c mptcpd_flags_t variable as needed, e.g.:
 *
 * @code
 * mptcpd_flags_t flags =
 *     MPTCPD_ADDR_FLAG_SUBFLOW | MPTCPD_ADDR_FLAG_BACKUP;
 * @endcode
 */
///@{
/**
 * @brief MPTCP flags type.
 *
 * MPTCP address flags integer type that contains set of flag bits.
 */
typedef uint32_t mptcpd_flags_t;

/// Trigger announcement of a new local IP address.
#define MPTCPD_ADDR_FLAG_SIGNAL  (1U << 0)

/// Create a new subflow.
#define MPTCPD_ADDR_FLAG_SUBFLOW (1U << 1)

/// Set backup priority on the subflow.
#define MPTCPD_ADDR_FLAG_BACKUP  (1U << 2)
///@}

/// Notify even the addresses already existing at startup-time.
#define MPTCPD_NOTIFY_FLAG_EXISTING  (1U << 0)

/// Ignore link-local addresses.
#define MPTCPD_NOTIFY_FLAG_SKIP_LL (1U << 1)

/// Ignore host (loopback) addresses.
#define MPTCPD_NOTIFY_FLAG_SKIP_HOST (1U << 2)

/**
 * @enum mptcpd_limit_types
 *
 * @brief MPTCP resource limit type identifiers.
 */
enum mptcpd_limit_types
{
        /// Maximum number of address advertisements to receive.
        MPTCPD_LIMIT_RCV_ADD_ADDRS,

        /// Maximum number of subflows.
        MPTCPD_LIMIT_SUBFLOWS
};

struct mptcpd_addr_info;

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

/**
 * @brief Type of function called when an address is available.
 *
 * The mptcpd path manager will call a function of this type when
 * the result of calling @c mptcpd_pm_get_addr() or
 * @c mptcpd_pm_dump_addrs() is available.
 *
 * @param[in]     info          Network address information.  @c NULL
 *                              on error.
 * @param[in,out] callback_data Data provided by the caller of
 *                              @c mptcpd_pm_get_addr() or
 *                              @c mptcpd_pm_dump_addrs().
 */
typedef void (*mptcpd_pm_get_addr_cb)(struct mptcpd_addr_info const *info,
                                      void *callback_data);

/**
 * @brief Type of function called when MPTCP resource limits are
 *        available.
 *
 * The mptcpd path manager will call a function of this type when
 * the result of calling @c mptcpd_pm_get_limits() is available.
 *
 * @param[in]     limits        Array of MPTCP resource type/limit
 *                              pairs.  @c NULL on error.
 * @param[in]     len           Length of the @a limits array.  Zero
 *                              on error.
 * @param[in,out] callback_data Data provided by the caller of
 *                              @c mptcpd_pm_get_limits().
 */
typedef void (*mptcpd_pm_get_limits_cb)(
        struct mptcpd_limit const *limits,
        size_t len,
        void *callback_data);

#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_TYPES_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
