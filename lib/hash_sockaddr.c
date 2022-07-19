// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file hash_sockaddr.c
 *
 * @brief @c struct @c sockaddr hash related functions.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <stdint.h>
#include <string.h>
#include <netinet/in.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/util.h>
#pragma GCC diagnostic pop


#include <mptcpd/private/murmur_hash.h>

#include "hash_sockaddr.h"


static int compare_sockaddr_in(struct sockaddr const *lsa,
                        struct sockaddr const *rsa)
{
        struct sockaddr_in const *const lin =
                (struct sockaddr_in const *) lsa;

        struct sockaddr_in const *const rin =
                (struct sockaddr_in const *) rsa;

        in_addr_t const lhs = lin->sin_addr.s_addr;
        in_addr_t const rhs = rin->sin_addr.s_addr;

        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static int compare_sockaddr_in6(struct sockaddr const *lsa,
                                struct sockaddr const *rsa)
{
        /**
         * @todo Should we compare the other @c struct @c sockaddr_in6
         *       fields as well?
         */

        struct sockaddr_in6 const *const lin =
                (struct sockaddr_in6 const *) lsa;

        struct sockaddr_in6 const *const rin =
                (struct sockaddr_in6 const *) rsa;

        uint8_t const *const lhs = lin->sin6_addr.s6_addr;
        uint8_t const *const rhs = rin->sin6_addr.s6_addr;

        return memcmp(lhs, rhs, sizeof(lin->sin6_addr.s6_addr));
}

int mptcpd_hash_sockaddr_compare(void const *a, void const *b)
{
        struct mptcpd_hash_sockaddr_key const *const lkey = a;
        struct mptcpd_hash_sockaddr_key const *const rkey = b;

        struct sockaddr const *const lsa = lkey->sa;
        struct sockaddr const *const rsa = rkey->sa;

        if (lsa->sa_family == rsa->sa_family) {
                if (lsa->sa_family == AF_INET) {
                        // IPv4
                        return compare_sockaddr_in(lsa, rsa);
                } else {
                        // IPv6
                        return compare_sockaddr_in6(lsa, rsa);
                }
        } else if (lsa->sa_family == AF_INET) {
                return 1;   // IPv4 > IPv6
        } else {
                return -1;  // IPv6 < IPv4
        }
}

void *mptcpd_hash_sockaddr_key_copy(void const *p)
{
        struct mptcpd_hash_sockaddr_key const *const src = p;
        struct mptcpd_hash_sockaddr_key *const key =
                l_new(struct mptcpd_hash_sockaddr_key, 1);

        struct sockaddr *sa = NULL;

        if (src->sa->sa_family == AF_INET) {
                sa = (struct sockaddr *) l_new(struct sockaddr_in, 1);

                memcpy(sa, src->sa, sizeof(struct sockaddr_in));
        } else {
                sa = (struct sockaddr *) l_new(struct sockaddr_in6, 1);

                memcpy(sa, src->sa, sizeof(struct sockaddr_in6));
        }

        key->sa = sa;
        key->seed = src->seed;

        return key;
}

void mptcpd_hash_sockaddr_key_free(void *p)
{
        if (p == NULL)
                return;

        struct mptcpd_hash_sockaddr_key *const key = p;

        l_free((void *) key->sa);  // Cast "const" away.
        l_free(key);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
