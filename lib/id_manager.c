// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file id_manager.c
 *
 * @brief Map of network address to MPTCP address ID.
 *
 * Copyright (c) 2020-2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ell/ell.h>

#include <mptcpd/private/murmur_hash.h>
#include <mptcpd/private/id_manager.h>
#include <mptcpd/id_manager.h>

#include "hash_sockaddr.h"

/// Invalid MPTCP address ID.
#define MPTCPD_INVALID_ID 0

/// Minimum MPTCP address ID.
#define MPTCPD_MIN_ID 1

/// Maximum MPTCP address ID.
#define MPTCPD_MAX_ID UINT8_MAX

/**
 * @struct mptcpd_idm
 *
 * @brief Internal mptcpd address ID manager data.
 */
struct mptcpd_idm
{
        /// Set of used MPTCP address IDs.
        struct l_uintset *ids;

        /**
         * @brief Map of IP address to MPTCP address ID.
         *
         * @todo A hashmap may be overkill for this use case since a
         *       given host isn't likely to have many local IP
         *       addresses.  Assuming that is the case, a simple O(n)
         *       based lookup on a linked list (e.g. @c l_queue) or an
         *       array should perform just as well.
         */
        struct l_hashmap *map;

        /// MurmurHash3 seed value.
        uint32_t seed;
};

// ----------------------------------------------------------------------

static bool mptcpd_hashmap_replace(struct l_hashmap *map,
                                   void const *key,
                                   void *value,
                                   void **old_value)
{
#ifdef HAVE_L_HASHMAP_REPLACE
        return l_hashmap_replace(map, key, value, old_value);
#else
        void *const old = l_hashmap_remove(map, key);

        if (old_value != NULL)
                *old_value = old;

        return l_hashmap_insert(map, key, value);
#endif
}

// ----------------------------------------------------------------------

static inline
unsigned int hash_sockaddr_in(struct sockaddr_in const *sa,
                                      uint32_t seed)
{
        return mptcpd_murmur_hash3(&sa->sin_addr.s_addr,
                                   sizeof(sa->sin_addr.s_addr),
                                   seed);
}

static inline
unsigned int hash_sockaddr_in6(struct sockaddr_in6 const *sa,
                               uint32_t seed)
{
        /**
         * @todo Should we include other sockaddr_in6 members, e.g.
         *       sin6_flowinfo and sin6_scope_id, as part of the key?
         */

        return mptcpd_murmur_hash3(sa->sin6_addr.s6_addr,
                                   sizeof(sa->sin6_addr.s6_addr),
                                   seed);
}

/**
 * @brief Generate a hash value based on IP address alone.
 *
 * @param[in] p @c struct @c mptcpd_hash_sockaddr_key instance
 *              containing the IP address to be hashed.
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

// ----------------------------------------------------------------------

struct mptcpd_idm *mptcpd_idm_create(void)
{
        struct mptcpd_idm *idm = l_new(struct mptcpd_idm, 1);

        assert(MPTCPD_MIN_ID != MPTCPD_INVALID_ID);

        idm->ids = l_uintset_new_from_range(MPTCPD_MIN_ID, MPTCPD_MAX_ID);
        idm->map = l_hashmap_new();
        idm->seed = l_getrandom_uint32();

        if (!l_hashmap_set_hash_function(idm->map, hash_sockaddr)
            || !l_hashmap_set_compare_function(idm->map,
                                               mptcpd_hash_sockaddr_compare)
            || !l_hashmap_set_key_copy_function(idm->map,
						mptcpd_hash_sockaddr_key_copy)
            || !l_hashmap_set_key_free_function(idm->map,
                                                mptcpd_hash_sockaddr_key_free)) {
                mptcpd_idm_destroy(idm);
                idm = NULL;
        }

        return idm;
}

void mptcpd_idm_destroy(struct mptcpd_idm *idm)
{
        if (idm == NULL)
                return;

        l_hashmap_destroy(idm->map, NULL);
        l_uintset_free(idm->ids);

        l_free(idm);
}

bool mptcpd_idm_map_id(struct mptcpd_idm *idm,
                       struct sockaddr const *sa,
                       mptcpd_aid_t id)
{
        if (idm == NULL || sa == NULL)
                return false;

        if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
                return false;

        if (id == MPTCPD_INVALID_ID
            || !l_uintset_put(idm->ids, id))
                return false;

        struct mptcpd_hash_sockaddr_key const key = {
                .sa = sa, .seed = idm->seed
        };

        if (!mptcpd_hashmap_replace(idm->map,
                                    &key,
                                    L_UINT_TO_PTR(id),
                                    NULL)) {
                (void) l_uintset_take(idm->ids, id);

                return false;
        }

        return true;
}

mptcpd_aid_t mptcpd_idm_get_id(struct mptcpd_idm *idm,
                               struct sockaddr const *sa)
{
        if (idm == NULL || sa == NULL)
                return MPTCPD_INVALID_ID;

        struct mptcpd_hash_sockaddr_key const key = {
                .sa = sa, .seed = idm->seed
        };

        // Check if an addr/ID mapping exists.
        uint32_t id = L_PTR_TO_UINT(l_hashmap_lookup(idm->map, &key));

        if (id != MPTCPD_INVALID_ID)
                return (mptcpd_aid_t) id;

        // Create a new addr/ID mapping.
        id = l_uintset_find_unused_min(idm->ids);

        if (id == MPTCPD_INVALID_ID || id == MPTCPD_MAX_ID + 1)
                return MPTCPD_INVALID_ID;

        if (!mptcpd_idm_map_id(idm, sa, id))
                return MPTCPD_INVALID_ID;

        return (mptcpd_aid_t) id;
}

mptcpd_aid_t mptcpd_idm_remove_id(struct mptcpd_idm *idm,
                                  struct sockaddr const *sa)
{
        if (idm == NULL || sa == NULL)
                return MPTCPD_INVALID_ID;

        struct mptcpd_hash_sockaddr_key const key = {
                .sa = sa, .seed = idm->seed
        };

        mptcpd_aid_t const id =
                L_PTR_TO_UINT(l_hashmap_remove(idm->map, &key));

        if (id == 0 || !l_uintset_take(idm->ids, id))
                return MPTCPD_INVALID_ID;

        return id;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
