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

#include <string.h>

#include <mptcpd/types.h>
#include <mptcpd/plugin.h>

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
        int new_address;
        int new_subflow;
        int subflow_closed;
        int connection_closed;
        //@}
};

inline void plugin_call_count_reset(struct plugin_call_count *p)
{
        p->new_connection    = 0;
        p->new_address       = 0;
        p->new_subflow       = 0;
        p->subflow_closed    = 0;
        p->connection_closed = 0;
}

inline bool plugin_call_count_is_sane(struct plugin_call_count const *lhs,
                                      struct plugin_call_count const *rhs)
{
        return     lhs->new_connection >= 0
                && lhs->new_address >= 0
                && lhs->new_subflow >= 0
                && lhs->subflow_closed >= 0
                && lhs->connection_closed >= 0

                && rhs->new_connection >= 0
                && rhs->new_address >= 0
                && rhs->new_subflow >= 0
                && rhs->subflow_closed >= 0
                && rhs->connection_closed >= 0

                && lhs->connection_closed <= rhs->new_connection
                && lhs->subflow_closed <= rhs->new_subflow;
}

inline bool plugin_call_count_is_equal(
        struct plugin_call_count const *lhs,
        struct plugin_call_count const *rhs)
{
        return     lhs->new_connection    == rhs->new_connection
                && lhs->new_address       == rhs->new_address
                && lhs->new_subflow       == rhs->new_subflow
                && lhs->subflow_closed    == rhs->subflow_closed
                && lhs->connection_closed == rhs->connection_closed;
}

// ------------------------------------------------------------------

/**
 * @name Plugin Operation Call Signatures
 *
 * Each test plugin will be called with a specific set of operations,
 * i.e. a "call signature", to verify that only the operations we
 * intended to call were actually called.
 */
//@{
struct plugin_call_count const test_count_1 = {
        .new_connection    = 1,
        .new_address       = 1,
        .new_subflow       = 0,
        .subflow_closed    = 0,
        .connection_closed = 1
};

struct plugin_call_count const test_count_2 = {
        .new_connection    = 1,
        .new_address       = 0,
        .new_subflow       = 1,
        .subflow_closed    = 1,
        .connection_closed = 1
};

struct plugin_call_count const test_count_4 = {
        .new_connection    = 1,
        .new_address       = 0,
        .new_subflow       = 0,
        .subflow_closed    = 0,
        .connection_closed = 1
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
mptcpd_cid_t const test_cid_1      = 0x12345678;
mptcpd_aid_t const test_laddr_id_1 = 0x34;
mptcpd_aid_t const test_raddr_id_1 = 0x56;
bool         const test_backup_1   = true;

mptcpd_cid_t const test_cid_2      = 0x23456789;
mptcpd_aid_t const test_laddr_id_2 = 0x23;
mptcpd_aid_t const test_raddr_id_2 = 0x45;
bool         const test_backup_2   = false;

mptcpd_cid_t const test_cid_4      = 0x34567890;
mptcpd_aid_t const test_laddr_id_4 = 0x90;
mptcpd_aid_t const test_raddr_id_4 = 0x01;
bool         const test_backup_4   = true;

struct mptcpd_addr const test_laddr_1 = {
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x34567890 } } },
        .port    = 0x1234
};

struct mptcpd_addr const test_laddr_2 = {
#ifdef __cplusplus
        /*
          s6_addr is a macro that expands to code that isn't
          compatible with  g++'s minimal support for C99 designated
          initializers.  Use IPv4 address instead.
        */
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x45678901 } } },
#else
        .address = { .family = AF_INET6,
                     .addr   = {
                        .addr6 = {
                                .s6_addr = { [0]  = 0x0,
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
                                             [15] = 0xF
                                }
                        }
                }
        },
#endif  // __cplusplus
        .port    = 0x5678
};

struct mptcpd_addr const test_raddr_1 = {
#ifdef __cplusplus
        /*
          s6_addr is a macro that expands to code that isn't
          compatible with  g++'s minimal support for C99 designated
          initializers.  Use IPv4 address instead.
        */
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x56789012 } } },
#else
        .address = { .family = AF_INET6,
                     .addr   = {
                        .addr6 = {
                                .s6_addr = { [0]  = 0xF,
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
                                             [15] = 0x0
                                }
                        }
                }
        },
#endif  // __cplusplus
        .port    = 0x3456
};

struct mptcpd_addr const test_raddr_2 = {
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x98765432 } } },
        .port    = 0x7890
};

struct mptcpd_addr const test_laddr_4 = {
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x45678901 } } },
        .port    = 0x2345
};

struct mptcpd_addr const test_raddr_4 = {
        .address = { .family = AF_INET,
                     .addr   = { .addr4 = { .s_addr = 0x56789012 } } },
        .port    = 0x3456
};

//@}

/**
 * @brief Compare equality of two @c mptcpd_addr objects.
 */
inline bool mptcpd_addr_is_equal(struct mptcpd_addr const *lhs,
                                 struct mptcpd_addr const *rhs)
{
        if (lhs->address.family == rhs->address.family
            && lhs->port == rhs->port) {
                struct mptcpd_in_addr const *const left =
                        &lhs->address;
                struct mptcpd_in_addr const *const right =
                        &rhs->address;

                if (lhs->address.family == AF_INET)
                        return left->addr.addr4.s_addr
                                == right->addr.addr4.s_addr;
                else
                        // memcmp() is fine for this case.
                        return memcmp(
                                &left->addr.addr6.s6_addr,
                                &right->addr.addr6.s6_addr,
                                sizeof(left->addr.addr6.s6_addr)) == 0;
        }

        return false;
}

inline void call_plugin_ops(struct plugin_call_count const *count,
                            char const *name,
                            mptcpd_cid_t connection_id,
                            mptcpd_aid_t laddr_id,
                            mptcpd_aid_t raddr_id,
                            struct mptcpd_addr const *laddr,
                            struct mptcpd_addr const *raddr,
                            bool backup)
{
        assert(count != NULL);

        for (int i = 0; i < count->new_connection; ++i)
                mptcpd_plugin_new_connection(name,
                                             connection_id,
                                             laddr,
                                             raddr,
                                             backup,
                                             NULL);

        for (int i = 0; i < count->new_address; ++i)
                mptcpd_plugin_new_address(connection_id,
                                          raddr_id,
                                          raddr,
                                          NULL);

        for (int i = 0; i < count->new_subflow; ++i)
                mptcpd_plugin_new_subflow(connection_id,
                                          laddr_id,
                                          laddr,
                                          raddr_id,
                                          raddr,
                                          NULL);

        for (int i = 0; i < count->subflow_closed; ++i)
                mptcpd_plugin_subflow_closed(connection_id,
                                             laddr,
                                             raddr,
                                             NULL);

        for (int i = 0; i < count->connection_closed; ++i)
                mptcpd_plugin_connection_closed(connection_id, NULL);
}


#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_TEST_PLUGIN_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
