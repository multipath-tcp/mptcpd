// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file tests/lib/interface.c
 *
 * @brief @c mptcpd_interface related utility functions.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>  // For NDEBUG
#endif

#include <assert.h>

#include <net/if_arp.h>
#include <net/if.h>
#include <netinet/in.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/util.h>
#include <ell/queue.h>
#pragma GCC diagnostic pop

#include <mptcpd/network_monitor.h>

#include "interface.h"


static int
addr_compare(void const *a, void const *b, void *user_data)
{
        (void) a;
        (void) b;
        (void) user_data;

        // No need to sort.
        return 1;
}

// -----------------------------------------------------------------------

struct mptcpd_interface *test_mptcpd_interface_create(void)
{
        struct mptcpd_interface *const i =
                l_new(struct mptcpd_interface, 1);

        // Bind test IP address to loopback interface.
        static char const loopback[] = "lo";

        i->family = AF_UNSPEC;
        i->type   = ARPHRD_LOOPBACK;
        i->flags  = IFF_UP | IFF_LOOPBACK;

        i->index  = if_nametoindex(loopback);
        assert(i->index != 0);

        l_strlcpy(i->name, loopback, L_ARRAY_SIZE(i->name));

        i->addrs = l_queue_new();

        return i;
}

bool test_mptcpd_interface_insert_addr(struct mptcpd_interface *i,
                                       struct sockaddr const *sa)
{
        assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

        struct sockaddr *addr = NULL;

        if (sa->sa_family == AF_INET)
                addr = l_memdup(sa, sizeof(struct sockaddr_in));
        else
                addr = l_memdup(sa, sizeof(struct sockaddr_in6));

        /*
          The mptcpd network monitor actually inserts an instance of
          the internal struct nm_addr_info into this list but it is
          actually expected for users to treat the list as a list of
          struct sockaddr const* objects.  This works since the first
          member of struct nm_addr_info is a struct sockaddr_storage,
          meaning the latter will have the same address as the
          former.  Just treat this list as list of struct sockaddr*
          for the purposes of the mptcpd unit test suite.
         */
        return l_queue_insert(i->addrs, addr, addr_compare, NULL);
}

void test_mptcpd_interface_destroy(struct mptcpd_interface *i)
{
        l_queue_destroy(i->addrs, l_free);
        l_free(i);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
