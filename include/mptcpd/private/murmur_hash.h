// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file murmur_hash.h
 *
 * This file contains a customized version of the public domain
 * MurmurHash3 code written by Austin Appleby.  Changes relative to
 * the original:
 *
 * @li 128 bit hash functions were removed.  They are not needed by
 *     mptcpd since the ELL l_hashmap expects hash values of type
 *     @c unsigned @c int.
 * @li The @c MurmurHash3_x86_32() function was renamed to
 *     @c mptcpd_murmur_hash3().
 * @li The hash value is returned as an @ @c unsigned @c int return
 *     value instead of a @c void* function "out" parameter.
 * @li The coding style was modified to conform to the mptcpd style.
 *
 * @see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.h
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifndef MPTCPD_MURMUR_HASH_3_H
#define MPTCPD_MURMUR_HASH_3_H

#include <stdint.h>

#include <mptcpd/export.h>


#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Generate hash of @a key using the MurmurHash3 algorithm.
 *
 * @param[in] key  Key value to be hashed.
 * @param[in] len  Length of @a key in bytes.
 * @param[in] seed Initial value of hash prior to hashing @a key.
 *
 * @return Hash of @a key.
 *
 * @attention The generated hash value is not cryptographically
 *            strong.
 *
 * @see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 */
MPTCPD_API unsigned int mptcpd_murmur_hash3(void const *key,
                                            int len,
                                            uint32_t seed);

#ifdef __cplusplus
}
#endif

#endif // MPTCPD_MURMUR_HASH_3_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
