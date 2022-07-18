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


/**
 * @brief Bundle IPv4 address and port as a hash key.
 *
 * A @c padding member is added to allow the padding bytes to be
 * initialized to zero in a designated initializer, e.g.:
 * @code
 * struct endpoint_in endpoint = {
 *     .addr = 0xC0000202,
 *     .port = 0x4321
 * };
 * The goal is to avoid hashing uninitialized bytes in the hash key.
 * @endcode
 */
struct key_in
{
        in_addr_t const addr;
        in_port_t const port;
        uint16_t  const padding;    // Initialize to zero.
};

/**
 * @brief Bundle IPv6 address and port as a hash key.
 *
 * A @c padding member is added to allow the padding bytes to be
 * initialized to zero in a designated initializer, e.g.:
 * @code
 * struct endpoint_in6 endpoint = {
 *     .addr = { .s6_addr = { [0]  = 0x20,
 *                            [1]  = 0x01,
 *                            [2]  = 0X0D,
 *                            [3]  = 0xB8,
 *                            [14] = 0x01,
 *                            [15] = 0x02 } },
 *     .port = 0x4321
 * };
 * The goal is to avoid hashing uninitialized bytes in the hash key.
 * @endcode
 */
struct key_in6
{
        struct in6_addr addr;
        in_port_t const port;
        uint16_t  const padding;  // Initialize to zero.
};

static unsigned int
mptcpd_hash_sockaddr_in(struct sockaddr_in const *sa,
                        uint32_t seed)
{
        // No port set.  Don't bother including it in the hash.
        if (sa->sin_port == 0)
                return mptcpd_murmur_hash3(&sa->sin_addr.s_addr,
                                           sizeof(sa->sin_addr.s_addr),
                                           seed);

        // ---------------------------------------

        // Non-zero port.  Include it in the hash.
        struct key_in const key = {
                .addr = sa->sin_addr.s_addr,
                .port = sa->sin_port
        };

        return mptcpd_murmur_hash3(&key, sizeof(key), seed);
}

static unsigned int
mptcpd_hash_sockaddr_in6(struct sockaddr_in6 const *sa,
                         uint32_t seed)
{
        /**
         * @todo Should we include other sockaddr_in6 members, e.g.
         *       sin6_flowinfo and sin6_scope_id, as part of the key?
         */

        // No port set.  Don't bother including it in the hash.
        if (sa->sin6_port == 0)
                return mptcpd_murmur_hash3(sa->sin6_addr.s6_addr,
                                           sizeof(sa->sin6_addr.s6_addr),
                                           seed);

        // ---------------------------------------

        // Non-zero port.  Include it in the hash.
        struct key_in6 key = { .port = sa->sin6_port };

        memcpy(key.addr.s6_addr,
               sa->sin6_addr.s6_addr,
               sizeof(key.addr.s6_addr));

        return mptcpd_murmur_hash3(&key, sizeof(key), seed);
}

unsigned int mptcpd_hash_sockaddr(void const *p)
{
        struct mptcpd_hash_sockaddr_key const *const key = p;
        struct sockaddr const *const sa = key->sa;

        assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

        if (sa->sa_family == AF_INET) {
                struct sockaddr_in const *sa4 =
                        (struct sockaddr_in const *) sa;

                return mptcpd_hash_sockaddr_in(sa4, key->seed);
        } else {
                struct sockaddr_in6 const *sa6 =
                        (struct sockaddr_in6 const *) sa;

                return mptcpd_hash_sockaddr_in6(sa6, key->seed);
        }
}

static inline int compare_port(in_port_t lhs, in_port_t rhs)
{
        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

static int compare_sockaddr_in(struct sockaddr const *lsa,
                        struct sockaddr const *rsa)
{
        struct sockaddr_in const *const lin =
                (struct sockaddr_in const *) lsa;

        struct sockaddr_in const *const rin =
                (struct sockaddr_in const *) rsa;

        in_addr_t const lhs = lin->sin_addr.s_addr;
        in_addr_t const rhs = rin->sin_addr.s_addr;

        if (lhs == rhs)
                return compare_port(lin->sin_port, rin->sin_port);

        return lhs < rhs ? -1 : 1;
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

        int const cmp = memcmp(lhs, rhs, sizeof(lin->sin6_addr.s6_addr));

        if (cmp == 0)
                return compare_port(lin->sin6_port, rin->sin6_port);

        return cmp;
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
