// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file sockaddr.c
 *
 * @brief @c sockaddr related test functions.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#include <string.h>

#include "test-plugin.h"


bool sockaddr_is_equal(struct sockaddr const *lhs,
                       struct sockaddr const *rhs)
{
        if (lhs->sa_family != rhs->sa_family)
                return false;

        // IPv4
        if (lhs->sa_family == AF_INET) {
                struct sockaddr_in const *const l =
                        (struct sockaddr_in const *) lhs;
                struct sockaddr_in const *const r =
                        (struct sockaddr_in const *) rhs;

                return l->sin_port == r->sin_port
                        && l->sin_addr.s_addr == r->sin_addr.s_addr;
        }

        // IPv6
        struct sockaddr_in6 const *const l =
                (struct sockaddr_in6 const *) lhs;
        struct sockaddr_in6 const *const r =
                (struct sockaddr_in6 const *) rhs;

        return l->sin6_port == r->sin6_port
                && memcmp(&l->sin6_addr,
                          &r->sin6_addr,
                          sizeof(l->sin6_addr)) == 0;
}

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
