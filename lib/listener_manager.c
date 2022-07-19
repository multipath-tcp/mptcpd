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
#include <ell/random.h>
#pragma GCC diagnostic pop

#include <mptcpd/private/murmur_hash.h>
#include <mptcpd/listener_manager.h>

#include "hash_sockaddr.h"


// ----------------------------------------------------------------------

/**
 * @struct mptcpd_lm
 *
 * @brief Internal mptcpd listern manager data.
 */
struct mptcpd_lm
{
        /// Map of @c struct @c sockaddr to listener file descriptor.
        struct l_hashmap *map;

        /// MurmurHash3 seed value.
        uint32_t seed;
};

// ----------------------------------------------------------------------

/**
 * @struct lm_value
 *
 * @brief mptcpd listener map entry value.
 */
struct lm_value
{
        /// Listener file descriptor.
        int fd;

        /**
         * @brief Listener reference count.
         *
         * Mptcpd listeners are reference counted to allow sharing.
         */
        int refcnt;
};

// ----------------------------------------------------------------------

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

static unsigned int hash_sockaddr_in(struct sockaddr_in const *sa,
                                     uint32_t seed)
{
        struct key_in const key = {
                .addr = sa->sin_addr.s_addr,
                .port = sa->sin_port
        };

        return mptcpd_murmur_hash3(&key, sizeof(key), seed);
}

static unsigned int hash_sockaddr_in6(struct sockaddr_in6 const *sa,
                                      uint32_t seed)
{
        /**
         * @todo Should we include other sockaddr_in6 members, e.g.
         *       sin6_flowinfo and sin6_scope_id, as part of the key?
         */
        struct key_in6 key = { .port = sa->sin6_port };

        memcpy(key.addr.s6_addr,
               sa->sin6_addr.s6_addr,
               sizeof(key.addr.s6_addr));

        return mptcpd_murmur_hash3(&key, sizeof(key), seed);
}

/**
 * @brief Generate a hash value based on IP address and port.
 *
 * @param[in] p @c struct @c mptcpd_hash_sockaddr_key instance
 *              containing the IP address and port to be hashed.
 *
 * @return The hash value.
 */
static unsigned int hash_sockaddr(void const *p)
{
        struct mptcpd_hash_sockaddr_key const *const key = p;
        struct sockaddr const *const sa = key->sa;

        assert(sa->sa_family == AF_INET || sa->sa_family == AF_INET6);

        if (sa->sa_family == AF_INET) {
                struct sockaddr_in const *sa4 =
                        (struct sockaddr_in const *) sa;

                return hash_sockaddr_in(sa4, key->seed);
        } else {
                struct sockaddr_in6 const *sa6 =
                        (struct sockaddr_in6 const *) sa;

                return hash_sockaddr_in6(sa6, key->seed);
        }
}

static inline int compare_port(in_port_t lhs, in_port_t rhs)
{
        return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

/**
 * @brief Compare hash map keys based on IP address and port.
 *
 * @param[in] a Pointer @c struct @c sockaddr (left hand side).
 * @param[in] b Pointer @c struct @c sockaddr (right hand side).
 *
 * @return 0 if the IP addresses and ports are equal, and -1 or 1
 *         otherwise, depending on IP address family and comparisons
 *         of IP addresses and ports of the same type.
 */
static int hash_sockaddr_compare(void const *a, void const *b)
{
        int const cmp = mptcpd_hash_sockaddr_compare(a, b);

        if (cmp != 0)
                return cmp;

        struct mptcpd_hash_sockaddr_key const *const lkey = a;
        struct mptcpd_hash_sockaddr_key const *const rkey = b;

        struct sockaddr const *const lsa = lkey->sa;
        struct sockaddr const *const rsa = rkey->sa;

        in_port_t lport, rport;

        if (lsa->sa_family == AF_INET) {
                struct sockaddr_in const *const lin =
                        (struct sockaddr_in const *) lsa;
                struct sockaddr_in const *const rin =
                        (struct sockaddr_in const *) rsa;

                lport = lin->sin_port;
                rport = rin->sin_port;
        } else {
                struct sockaddr_in6 const *const lin =
                        (struct sockaddr_in6 const *) lsa;
                struct sockaddr_in6 const *const rin =
                        (struct sockaddr_in6 const *) rsa;

                lport = lin->sin6_port;
                rport = rin->sin6_port;
        }

        return compare_port(lport, rport);
}

// ----------------------------------------------------------------------

static int open_listener(struct sockaddr const *sa)
{
#ifndef IPPROTO_MPTCP
#define IPPROTO_MPTCP IPPROTO_TCP + 256
#endif

        int const fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_MPTCP);
        if (fd == -1) {
                l_error("Unable to open MPTCP listener: %s",
                        strerror(errno));

                return -1;
        }

        socklen_t const addr_size =
                (sa->sa_family == AF_INET
                 ? sizeof(struct sockaddr_in)
                 : sizeof(struct sockaddr_in6));

        if (bind(fd, sa, addr_size) == -1) {
                l_error("Unable to bind MPTCP listener: %s",
                        strerror(errno));
                (void) close(fd);
                return -1;
        }

        if (listen(fd, 0) == -1) {
                l_error("Unable to listen on MPTCP socket: %s",
                        strerror(errno));
                (void) close(fd);
                return -1;
        }

        return fd;
}

static void close_listener(void *value)
{
        struct lm_value *const data = value;

        /**
         * @todo Should care if the @c close() call fails?  It
         *       shouldn't fail since we only get here if the listener
         *       socket was successfully opened earlier.
         */
        (void) close(data->fd);
        l_free(data);
}

// ----------------------------------------------------------------------

struct mptcpd_lm *mptcpd_lm_create(void)
{
        struct mptcpd_lm *lm = l_new(struct mptcpd_lm, 1);

        // Map of IP address to MPTCP listener file descriptor.
        lm->map  = l_hashmap_new();
        lm->seed = l_getrandom_uint32();

        if (!l_hashmap_set_hash_function(lm->map, hash_sockaddr)
            || !l_hashmap_set_compare_function(lm->map,
                                               hash_sockaddr_compare)
            || !l_hashmap_set_key_copy_function(lm->map,
						mptcpd_hash_sockaddr_key_copy)
            || !l_hashmap_set_key_free_function(lm->map,
                                                mptcpd_hash_sockaddr_key_free)) {
                mptcpd_lm_destroy(lm);
                lm = NULL;
        }

        return lm;
}

void mptcpd_lm_destroy(struct mptcpd_lm *lm)
{
        if (lm == NULL)
                return;

        l_hashmap_destroy(lm->map, close_listener);
        l_free(lm);
}

bool mptcpd_lm_listen(struct mptcpd_lm *lm, struct sockaddr const *sa)
{
        if (lm == NULL || sa == NULL)
                return false;

        if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
                return false;

        struct mptcpd_hash_sockaddr_key const key = {
                .sa = sa, .seed = lm->seed
        };

        struct lm_value *data = l_hashmap_lookup(lm->map, &key);

        /*
          A listener already exists for the given IP address.
          Increment the reference count.
        */
        if (data != NULL) {
                data->refcnt++;

                return true;
        }

        data = l_new(struct lm_value, 1);

        data->fd = open_listener(sa);
        if (data->fd == -1) {
                l_free(data);

                return false;
        }

        if (!l_hashmap_insert(lm->map, &key, data)) {
                l_error("Unable to map MPTCP address to listener.");

                (void) close(data->fd);
                l_free(data);

                return false;
        }

        data->refcnt = 1;

        return true;
}

bool mptcpd_lm_close(struct mptcpd_lm *lm, struct sockaddr const *sa)
{
        if (lm == NULL || sa == NULL)
                return false;

        struct mptcpd_hash_sockaddr_key const key = {
                .sa = sa, .seed = lm->seed
        };

        struct lm_value *const data = l_hashmap_lookup(lm->map, &key);

        /*
          A listener already exists for the given IP address.
          Decrement the reference count.
        */
        if (data == NULL)
                return false;

        if (--data->refcnt == 0) {
                // No more listeners sharing the same address.
                close_listener(data);
                (void) l_hashmap_remove(lm->map, &key);
        }

        return true;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
