// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file listener_manager.c
 *
 * @brief Map of MPTCP local address ID to listener.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/hashmap.h>
#include <ell/util.h>
#include <ell/log.h>
#pragma GCC diagnostic pop

#include "listener_manager.h"


// ----------------------------------------------------------------------

struct l_hashmap *mptcpd_lm_create(void)
{
        // Map of MPTCP local address ID to MPTCP listener file
        // descriptor.
        return l_hashmap_new();
}

static void close_listener(void *value)
{
        /*
          We don't need to worry about overflow due to casting from
          unsigned int since the file descriptor value stored in the
          map will never be larger than INT_MAX by design.
        */
        int const fd = L_PTR_TO_UINT(value);

        (void) close(fd);
}


void mptcpd_lm_destroy(struct l_hashmap *lm)
{
        if (lm == NULL)
                return;

        l_hashmap_destroy(lm, close_listener);
}

bool mptcpd_lm_listen(struct l_hashmap *lm,
                      mptcpd_aid_t id,
                      struct sockaddr const *sa)
{
        /**
         * @todo Allow id == 0?
         */
        if (lm == NULL || id == 0 || sa == NULL)
                return false;

        if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
                return false;

#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP IPPROTO_TCP + 256
#endif

        int const fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_MPTCP);
        if (fd == -1)
                l_error("Unable to open MPTCP listener: %s",
                        strerror(errno));

        socklen_t const addr_size =
                (sa->sa_family == AF_INET
                 ? sizeof(struct sockaddr_in)
                 : sizeof(struct sockaddr_in6));

        if (bind(fd, sa, addr_size) == -1) {
                l_error("Unable to bind MPTCP listener: %s",
                        strerror(errno));
                (void) close(fd);
                return false;
        }

        if (listen(fd, 0) == -1) {
                l_error("Unable to listen on MPTCP socket: %s",
                        strerror(errno));
                (void) close(fd);
                return false;
        }

        if (!l_hashmap_insert(lm, L_UINT_TO_PTR(id), L_UINT_TO_PTR(fd))) {
                l_error("Unable to map MPTCP address ID %u listener", id);
                (void) close(fd);
                return false;
        }

        return true;
}

bool mptcpd_lm_close(struct l_hashmap *lm, mptcpd_aid_t id)
{
        /**
         * @todo Allow id == 0?
         */
        if (lm == NULL || id == 0)
                return false;

        void const *const value = l_hashmap_remove(lm, L_UINT_TO_PTR(id));

        if (value == NULL) {
                l_error("No listener for MPTCP address id %u.", id);
                return false;
        }

        // Value will never exceed INT_MAX.
        int const fd = L_PTR_TO_UINT(value);

        return close(fd) == 0;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
