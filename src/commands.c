// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file commands.c
 *
 * @brief mptcpd generic netlink command utilities.
 *
 * Copyright (c) 2017-2020, 2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().

#include <string.h>
#include <arpa/inet.h>

#include <ell/ell.h>

#include "commands.h"


uint16_t mptcpd_get_port_number(struct sockaddr const *addr)
{
        in_port_t port = 0;

        if (addr == NULL)
                return port;

        if (addr->sa_family == AF_INET) {
                struct sockaddr_in const *const addr4 =
                        (struct sockaddr_in const*) addr;

                port = addr4->sin_port;

        } else if (addr->sa_family == AF_INET6) {
                struct sockaddr_in6 const *const addr6 =
                        (struct sockaddr_in6 const*) addr;

                port = addr6->sin6_port;
        }

        return ntohs(port);
}

bool mptcpd_check_genl_error(struct l_genl_msg *msg, char const *fname)
{
        int const error = l_genl_msg_get_error(msg);

        if (error < 0) {
                // Error during send.
                char const *const genl_errmsg =
#ifdef HAVE_L_GENL_MSG_GET_EXTENDED_ERROR
                        l_genl_msg_get_extended_error(msg);
#else
                        NULL;
#endif

                char errmsg[80] = { 0 };
                (void) strerror_r(-error, errmsg, L_ARRAY_SIZE(errmsg));

                if (genl_errmsg != NULL)
                        l_error("%s: %s: %s", fname, genl_errmsg, errmsg);
                else
                        l_error("%s: %s", fname, errmsg);

                return false;
        }

        return true;
}

void mptcpd_family_send_callback(struct l_genl_msg *msg, void *user_data)
{
        char const *const fname = user_data;

        (void) mptcpd_check_genl_error(msg, fname);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
