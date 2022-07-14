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

        if (!l_hashmap_set_hash_function(lm->map, mptcpd_hash_sockaddr)
            || !l_hashmap_set_compare_function(lm->map,
                                               mptcpd_hash_sockaddr_compare)
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
