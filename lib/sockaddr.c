// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file src/sockaddr.c
 *
 * @brief mptcpd @c struct @c sockaddr related utility functions.
 *
 * Copyright (c) 2019-2020, Intel Corporation
 */

#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <mptcpd/private/sockaddr.h>


bool mptcpd_sockaddr_storage_init(struct in_addr  const *addr4,
                                  struct in6_addr const *addr6,
                                  unsigned short port,
                                  struct sockaddr_storage *addr)
{
        if ((addr4 == NULL && addr6 == NULL) || addr == NULL)
                return false;

        if (addr4 != NULL) {
                struct sockaddr_in *const a = (struct sockaddr_in *) addr;

                a->sin_family      = AF_INET;
                a->sin_port        = port;
                a->sin_addr.s_addr = addr4->s_addr;
        } else {
                struct sockaddr_in6 *const a =
                        (struct sockaddr_in6 *) addr;

                a->sin6_family = AF_INET6;
                a->sin6_port   = port;

                memcpy(&a->sin6_addr, addr6, sizeof(*addr6));
        }

        return true;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
