// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file hash_sockaddr.h
 *
 * @brief @c struct @c sockaddr hash related functions.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifndef MPTCPD_HASH_SOCKADDR_H
#define MPTCPD_HASH_SOCKADDR_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;

/**
 * @name ELL Hash Functions For IP Addresses
 *
 * A set of types and functions for using an IP address through a
 * @c struct @c sockaddr as the key for an ELL hashmap (@c struct
 * @c l_hashmap).
 *
 * @note These functions are only used internally, and are not
 *       exported from libmptcpd.
 */
///@{
/**
 * @brief Hash key information.
 *
 * Bundle the values needed to generate a hash value based on an IP
 * address (@c struct @c sockaddr) using the MurmurHash3 algorithm.
 */
struct mptcpd_hash_sockaddr_key
{
        /// IP address to be hashed.
        struct sockaddr const *sa;

        /// Hash algorithm (MurmurHash3) seed.
        uint32_t seed;
};

/**
 * @brief Compare hash map keys based on IP address alone.
 *
 * @param[in] a Pointer @c struct @c sockaddr (left hand side).
 * @param[in] b Pointer @c struct @c sockaddr (right hand side).
 *
 * @return 0 if the IP addresses are equal, and -1 or 1 otherwise,
 *         depending on IP address family and comparisons of IP
 *         addresses of the same type.
 *
 * @note Ports are not compared.
 */
int mptcpd_hash_sockaddr_compare(void const *a, void const *b);

/**
 * @brief Deep copy the hash map key @a p.
 *
 * @return The dynamically allocated deep copy of the hash map key
 *         @a p.  Deallocate with @c l_free().
 */
void *mptcpd_hash_sockaddr_key_copy(void const *p);

/// Deallocate @struct @c mptcpd_hash_sockaddr_key instance.
void mptcpd_hash_sockaddr_key_free(void *p);
///@}

#ifdef __cplusplus
}
#endif


#endif // MPTCPD_HASH_SOCKADDR_H

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
