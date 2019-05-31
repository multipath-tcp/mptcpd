// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpd_addr.c
 *
 * @brief mptcpd_addr related test functions.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#include <string.h>

#include "test-plugin.h"


bool mptcpd_addr_is_equal(struct mptcpd_addr const *lhs,
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
