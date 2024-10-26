// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file lib/sockaddr.c
 *
 * @brief mptcpd @c struct @c sockaddr related utility functions.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ell/ell.h>

#include <mptcpd/private/sockaddr.h>


bool mptcpd_sockaddr_storage_init(in_addr_t const *addr4,
                                  struct in6_addr const *addr6,
                                  in_port_t port,
                                  struct sockaddr_storage *addr)
{
        if ((addr4 == NULL && addr6 == NULL) || addr == NULL)
                return false;

        if (addr4 != NULL) {
                struct sockaddr_in *const a = (struct sockaddr_in *) addr;

                a->sin_family      = AF_INET;
                a->sin_port        = port;
                a->sin_addr.s_addr = *addr4;
        } else {
                struct sockaddr_in6 *const a =
                        (struct sockaddr_in6 *) addr;

                a->sin6_family = AF_INET6;
                a->sin6_port   = port;

                memcpy(&a->sin6_addr, addr6, sizeof(*addr6));
        }

        return true;
}

struct sockaddr *
mptcpd_sockaddr_copy(struct sockaddr const *sa)
{
        if (sa == NULL)
                return NULL;

        if (sa->sa_family == AF_INET)
                return l_memdup(sa, sizeof(struct sockaddr_in));
        else if (sa->sa_family == AF_INET6)
                return l_memdup(sa, sizeof(struct sockaddr_in6));

        // Not an IPv4 or IPv6 address.
        return NULL;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
