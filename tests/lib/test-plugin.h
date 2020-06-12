// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-plugin.h
 *
 * @brief mptcpd plugin test header.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#ifndef MPTCPD_TEST_PLUGIN_H
#define MPTCPD_TEST_PLUGIN_H

#include <stdbool.h>

#include <netinet/in.h>

#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

// ------------------------------------------------------------------

/**
 * @struct plugin_call_count
 *
 * @brief Plugin operation call counts.
 *
 * The structure is used to track the number of times each mptcpd
 * plugin operation is called or should be called.
 */
struct plugin_call_count
{
        /**
         * @name Mptcpd Plugin Operation Call Counts
         *
         * Each of these fields corresponds to the callback operations
         * defined in @c mptcpd_plugin_ops that all mptcpd plugins
         * must implement.
         */
        //@{
        int new_connection;
        int connection_established;
        int connection_closed;
        int new_address;
        int address_removed;
        int new_subflow;
        int subflow_closed;
        int subflow_priority;
        //@}
};

/**
 * @brief Reset plugin operation call counts.
 */
void call_count_reset(struct plugin_call_count *p);

/**
 * @brief Check if all plugin operation call counts are not negative.
 */
bool call_count_all_positive(struct plugin_call_count const *p);

/**
 * @brief Perform a sanity check on the plugin operation call counts.
 */
bool call_count_is_sane(struct plugin_call_count const *p);

/**
 * @brief Compare plugin operation call counts for equality.
 */
bool call_count_is_equal(struct plugin_call_count const *lhs,
                         struct plugin_call_count const *rhs);

// ------------------------------------------------------------------

/**
 * @name Plugin Operation Call Signatures
 *
 * Each test plugin will be called with a specific set of operations,
 * i.e. a "call signature", to verify that only the operations we
 * intended to call were actually called.
 */
//@{
static struct plugin_call_count const test_count_1 = {
        .new_connection         = 1,
        .connection_established = 1,
        .connection_closed      = 1,
        .new_address            = 1,
        .address_removed        = 0,
        .new_subflow            = 0,
        .subflow_closed         = 0,
        .subflow_priority       = 0
};

static struct plugin_call_count const test_count_2 = {
        .new_connection         = 1,
        .connection_established = 1,
        .connection_closed      = 1,
        .new_address            = 0,
        .address_removed        = 1,
        .new_subflow            = 1,
        .subflow_closed         = 1,
        .subflow_priority       = 1
};

static struct plugin_call_count const test_count_4 = {
        .new_connection         = 1,
        .connection_established = 1,
        .connection_closed      = 1,
        .new_address            = 1,
        .address_removed        = 1,
        .new_subflow            = 0,
        .subflow_closed         = 0,
        .subflow_priority       = 0
};
//@}

// ------------------------------------------------------------------

/**
 * @name Plugin Operation Argument Test Values
 *
 * Arbitrary test values passed to test plugin operations.  We only
 * care about matching values, not if they are actually valid for the
 * types of values they correspond to.
 */
//@{
static mptcpd_token_t const test_token_1    = 0x12345678;
static mptcpd_aid_t   const test_laddr_id_1 = 0x34;
static mptcpd_aid_t   const test_raddr_id_1 = 0x56;
static bool           const test_backup_1   = true;

static mptcpd_token_t const test_token_2    = 0x23456789;
static mptcpd_aid_t   const test_laddr_id_2 = 0x23;
static mptcpd_aid_t   const test_raddr_id_2 = 0x45;
static bool           const test_backup_2   = false;

static mptcpd_token_t const test_token_4    = 0x34567890;
static mptcpd_aid_t   const test_laddr_id_4 = 0x90;
static mptcpd_aid_t   const test_raddr_id_4 = 0x01;
static bool           const test_backup_4   = true;

// For verifying that a plugin will not be dispatched.
static mptcpd_token_t const test_bad_token  = 0xFFFFFFFF;

static struct sockaddr_in const test_laddr_1 = {
        .sin_family = AF_INET,
        .sin_port   = 0x1234,
        .sin_addr   = { .s_addr = 0x34567890 }
};

#ifdef __cplusplus
/*
  s6_addr is a macro that expands to code that isn't compatible with
  g++'s minimal support for C99 designated initializers.  Use IPv4
  address instead.
*/
static struct sockaddr_in const test_laddr_2 = {
        .sin_family = AF_INET,
        .sin_port   = 0x5678,
        .sin_addr   = { .s_addr = 0x45678901 }
};
#else
static struct sockaddr_in6 const test_laddr_2 = {
        .sin6_family = AF_INET6,
        .sin6_port   = 0x5678,
        .sin6_addr   = { .s6_addr = { [0]  = 0x0,
                                      [1]  = 0x1,
                                      [2]  = 0x2,
                                      [3]  = 0x3,
                                      [4]  = 0x4,
                                      [5]  = 0x5,
                                      [6]  = 0x6,
                                      [7]  = 0x7,
                                      [8]  = 0x8,
                                      [9]  = 0x9,
                                      [10] = 0xA,
                                      [11] = 0xB,
                                      [12] = 0xC,
                                      [13] = 0xD,
                                      [14] = 0xE,
                                      [15] = 0xF }
        }
};
#endif  // __cplusplus


#ifdef __cplusplus
/*
  s6_addr is a macro that expands to code that isn't compatible with
  g++'s minimal support for C99 designated initializers.  Use IPv4
  address instead.
*/
static struct sockaddr_in const test_raddr_1 = {
        .sin_family = AF_INET,
        .sin_port   = 0x3456,
        .sin_addr   = { .s_addr = 0x56789012 }
};
#else
static struct sockaddr_in6 const test_raddr_1 = {
        .sin6_family = AF_INET6,
        .sin6_port   = 0x3456,
        .sin6_addr   = { .s6_addr = { [0]  = 0xF,
                                      [1]  = 0xE,
                                      [2]  = 0xD,
                                      [3]  = 0xC,
                                      [4]  = 0xB,
                                      [5]  = 0xA,
                                      [6]  = 0x9,
                                      [7]  = 0x8,
                                      [8]  = 0x7,
                                      [9]  = 0x6,
                                      [10] = 0x5,
                                      [11] = 0x4,
                                      [12] = 0x3,
                                      [13] = 0x2,
                                      [14] = 0x1,
                                      [15] = 0x0 }
        }
};
#endif  // __cplusplus

static struct sockaddr_in const test_raddr_2 = {
        .sin_family = AF_INET,
        .sin_port   = 0x7890,
        .sin_addr   = { .s_addr = 0x98765432 }
};

static struct sockaddr_in const test_laddr_4 = {
        .sin_family = AF_INET,
        .sin_port   = 0x2345,
        .sin_addr   = { .s_addr = 0x45678901 }
};

static struct sockaddr_in const test_raddr_4 = {
        .sin_family = AF_INET,
        .sin_port   = 0x3456,
        .sin_addr   = { .s_addr = 0x56789012 }
};

//@}

/**
 * @brief Compare equality of two @c sockaddr objects.
 *
 * @return @c true if both @c sockaddr objects are equal. @c false
 *         otherwise.
 */
bool sockaddr_is_equal(struct sockaddr const *lhs,
                       struct sockaddr const *rhs);

/**
 * @brief Call plugin operations
 *
 * @param[in] count    Number of times to call each plugin operation.
 * @param[in] name     Plugin name.
 * @param[in] raddr_id Remote address ID.
 * @param[in] laddr    Local address.
 * @param[in] raddr    Remote address.
 * @param[in] backup   MPTCP backup priority.
 */
void call_plugin_ops(struct plugin_call_count const *count,
                     char const *name,
                     mptcpd_token_t token,
                     mptcpd_aid_t raddr_id,
                     struct sockaddr const *laddr,
                     struct sockaddr const *raddr,
                     bool backup);


#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_TEST_PLUGIN_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
