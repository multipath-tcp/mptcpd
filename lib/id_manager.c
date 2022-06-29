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

#include <assert.h>
#include <stdint.h>
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>

#ifdef HAVE_GETAUXVAL
# include <sys/auxv.h>
#endif // HAVE_GETAUXVAL

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/hashmap.h>
#include <ell/uintset.h>
#include <ell/util.h>
#pragma GCC diagnostic pop

#include <mptcpd/private/murmur_hash.h>
#include <mptcpd/private/id_manager.h>
#include <mptcpd/id_manager.h>

/// Invalid MPTCP address ID.
#define MPTCPD_INVALID_ID 0

/// Minimum MPTCP address ID.
#define MPTCPD_MIN_ID 1

/// Maximum MPTCP address ID.
#define MPTCPD_MAX_ID UINT8_MAX


/**
 * @brief MurmurHash3 seed value.
 *
 * @note This could be tied to each @c mptcpd_idm instance so that the
 *       seed value wouldn't shared between multiple mptcpd_idm
 *       instances but the only way to pass the seed value to the
 *       mptcpd MurmurHash3 hash function through the ELL @c l_hashmap
 *       interface would be to make it a part of the key, such as
 *       through a convenience @c struct.  That would double the size
 *       of the key on 64 bit platforms (sizeof(struct sockaddr*) +
 *       sizeof(uint32_t) + padding).  That may not be a problem for
 *       the common case since most platforms would not have many
 *       local addresses, meaning the larger key size would not impact
 *       the run-time memory footprint by much.  Furthermore, it is
 *       unlikely that such a global seed value shared between
 *       @c mptcpd_idm instances would be a problem since each
 *       @c mptcpd_idm insance would have its own @c l_hashmap.
 */
static uint32_t _idm_hash_seed = 0;

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
};

// ----------------------------------------------------------------------

static inline unsigned int
mptcpd_hash_sockaddr_in(struct sockaddr_in const *sa)
{
        return mptcpd_murmur_hash3(&sa->sin_addr.s_addr,
                                   sizeof(sa->sin_addr.s_addr),
                                   _idm_hash_seed);
}

static inline unsigned int
mptcpd_hash_sockaddr_in6(struct sockaddr_in6 const *sa)
{
        return mptcpd_murmur_hash3(sa->sin6_addr.s6_addr,
                                   sizeof(sa->sin6_addr.s6_addr),
                                   _idm_hash_seed);
}

static unsigned int mptcpd_hash_sockaddr(void const *p)
{
        struct sockaddr const *const sa = p;
        if (sa->sa_family == AF_INET) {
                struct sockaddr_in const *sa4 =
                        (struct sockaddr_in const *) sa;

                return mptcpd_hash_sockaddr_in(sa4);
        } else {
                struct sockaddr_in6 const *sa6 =
                        (struct sockaddr_in6 const *) sa;

                return mptcpd_hash_sockaddr_in6(sa6);
        }
}

static int mptcpd_hashmap_compare(void const *a, void const *b)
{
        struct sockaddr const *const lsa = a;
        struct sockaddr const *const rsa = b;

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

static void *mptcpd_hashmap_key_copy(void const *p)
{
        struct sockaddr const *const sa = p;
        struct sockaddr *key = NULL;

        if (sa->sa_family == AF_INET) {
                key = (struct sockaddr *) l_new(struct sockaddr_in, 1);

                memcpy(key, sa, sizeof(struct sockaddr_in));
        } else {
                key = (struct sockaddr *) l_new(struct sockaddr_in6, 1);

                memcpy(key, sa, sizeof(struct sockaddr_in6));
        }

        return key;
}

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

struct mptcpd_idm *mptcpd_idm_create(void)
{
        struct mptcpd_idm *idm = l_new(struct mptcpd_idm, 1);

        assert(MPTCPD_MIN_ID != MPTCPD_INVALID_ID);

        idm->ids = l_uintset_new_from_range(MPTCPD_MIN_ID, MPTCPD_MAX_ID);

        idm->map = l_hashmap_new();

        if (!l_hashmap_set_hash_function(idm->map, mptcpd_hash_sockaddr)
            || !l_hashmap_set_compare_function(idm->map,
                                               mptcpd_hashmap_compare)
            || !l_hashmap_set_key_copy_function(idm->map,
						mptcpd_hashmap_key_copy)
            || !l_hashmap_set_key_free_function(idm->map, l_free)) {
                mptcpd_idm_destroy(idm);
                idm = NULL;
        }

        if (_idm_hash_seed == 0) {
                _idm_hash_seed = (uint32_t) time(NULL);

#ifdef HAVE_GETAUXVAL
                /*
                  Further randomize the hash seed value with
                  process-specific random data supplied by the kernel,
                  skipping the canary in the first half.
                */
                unsigned long const addr =
                        getauxval(AT_RANDOM) + sizeof(void *);

                _idm_hash_seed ^= (uint32_t) *((unsigned long *) addr);
#endif  // HAVE_GETAUXVAL
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

        if (!mptcpd_hashmap_replace(idm->map,
                                    sa,
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

        // Check if an addr/ID mapping exists.
        uint32_t id = L_PTR_TO_UINT(l_hashmap_lookup(idm->map, sa));

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

        mptcpd_aid_t const id =
                L_PTR_TO_UINT(l_hashmap_remove(idm->map, sa));

        if (id == 0 || !l_uintset_take(idm->ids, id))
                return MPTCPD_INVALID_ID;

        return id;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
