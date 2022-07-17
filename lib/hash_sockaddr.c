// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file hash_sockaddr.c
 *
 * @brief @c struct @c sockaddr hash related functions.
 *
 * Copyright (c) 2022, Intel Corporation
 */

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
 */
struct endpoint_in
{
        in_addr_t addr;
        in_port_t port;
};

/**
 * @brief Bundle IPv6 address and port as a hash key.
 */
struct endpoint_in6
{
        struct in6_addr addr;
        in_port_t port;
};

/**
 * @brief IPv4 address/port hash key buffer.
 *
 * This union ensures that padding bytes are initialized to zero in
 * the @c endpoint member.  For this to work the @c buffer member must
 * initialized prior to the @c endpoint member, e.g.
 * @code
 * union key_in key = { .buffer = { 0 } };
 *
 * key.endpoint.addr = sa->sin_addr.s_addr;
 * key.endpoint.port = sa->sin_port;
 * @endcode
 * The goal is to avoid hashing uninitialized bytes in the hash key.
 */
union key_in
{
        struct endpoint_in endpoint;
        uint8_t buffer[sizeof(struct endpoint_in)];
};

/**
 * @brief IPv6 address/port hash key buffer.
 *
 * This union ensures that padding bytes are initialized to zero in
 * the @c endpoint member.  For this to work the @c buffer member must
 * initialized prior to the @c endpoint member, e.g.
 * @code
 * union key_in6 key = { .buffer = { 0 } };
 *
 * memcpy(key.endpoint.addr.s6_addr,
 *        sa->sin6_addr.s6_addr,
 *        sizeof(key.endpoint.addr.s6_addr));
 *
 * key.endpoint.port = sa->sin6_port;
 * @endcode
 * The goal is to avoid hashing uninitialized bytes in the hash key.
 */
union key_in6
{
        struct endpoint_in6 endpoint;
        uint8_t buffer[sizeof(struct endpoint_in6)];
};


static unsigned int mptcpd_hash_sockaddr_in(struct sockaddr_in const *sa,
                        uint32_t seed)
{
        // No port set.  Don't bother including it in the hash.
        if (sa->sin_port == 0)
                return mptcpd_murmur_hash3(&sa->sin_addr.s_addr,
                                           sizeof(sa->sin_addr.s_addr),
                                           seed);

        // ---------------------------------------

        // Non-zero port.  Include it in the hash.

        /*
          Initialize the key buffer first to ensure padding bytes in
          the endpoint union member are zero.  This works since the
          buffer union member also includes padding bytes found in the
          endpoint member.
        */
        union key_in key = { .buffer = { 0 } };

        key.endpoint.addr = sa->sin_addr.s_addr;
        key.endpoint.port = sa->sin_port;

        return mptcpd_murmur_hash3(key.buffer, sizeof(key.buffer), seed);
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

        /*
          Initialize the key buffer first to ensure padding bytes in
          the endpoint union member are zero.  This works since the
          buffer union member also includes padding bytes found in the
          endpoint member.
        */
        union key_in6 key = { .buffer = { 0 } };

        memcpy(key.endpoint.addr.s6_addr,
               sa->sin6_addr.s6_addr,
               sizeof(key.endpoint.addr.s6_addr));

        key.endpoint.port = sa->sin6_port;

        return mptcpd_murmur_hash3(key.buffer, sizeof(key.buffer), seed);
}

unsigned int mptcpd_hash_sockaddr(void const *p)
{
        struct mptcpd_hash_sockaddr_key const *const key = p;
        struct sockaddr const *const sa = key->sa;

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

int mptcpd_hash_sockaddr_compare(void const *a, void const *b)
{
        struct mptcpd_hash_sockaddr_key const *const lkey = a;
        struct mptcpd_hash_sockaddr_key const *const rkey = b;

        struct sockaddr const *const lsa = lkey->sa;
        struct sockaddr const *const rsa = rkey->sa;

        /**
         * @todo Should we compare the other @c struct @c sockaddr_in
         *       and @c struct @c sockaddr_in6 fields as well?
         */

        if (lsa->sa_family == rsa->sa_family) {
                if (lsa->sa_family == AF_INET) {
                        // IPv4
                        struct sockaddr_in const *const lin =
                                (struct sockaddr_in const *) lsa;

                        struct sockaddr_in const *const rin =
                                (struct sockaddr_in const *) rsa;

                        uint32_t const lhs = lin->sin_addr.s_addr;
                        uint32_t const rhs = rin->sin_addr.s_addr;

                        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
                } else {
                        // IPv6
                        struct sockaddr_in6 const *const lin =
                                (struct sockaddr_in6 const *) lsa;

                        struct sockaddr_in6 const *const rin =
                                (struct sockaddr_in6 const *) rsa;

                        uint8_t const *const lhs = lin->sin6_addr.s6_addr;
                        uint8_t const *const rhs = rin->sin6_addr.s6_addr;

                        return memcmp(lhs,
                                      rhs,
                                      sizeof(lin->sin6_addr.s6_addr));
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
