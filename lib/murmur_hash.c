// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file murmur_hash.c
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
 * @li The hash value is returned as an @c unsigned @c int return
 *     value instead of a function "out" parameter of type @c void*.
 * @li The only compiler-specific support left in place is for gcc and
 *     clang.
 * @li The coding style was modified to conform to the mptcpd style.
 * @li gcc "implicit fallthrough" warnings for switch statement were
 *     silenced.
 *
 * @see https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <mptcpd/private/murmur_hash.h>

#ifdef __GNUC__
#  define FORCE_INLINE inline __attribute__((always_inline))

/*
  Silence gcc "implicit fallthrough" warnings for switch statement
  cases below.
*/
#  define FALLTHROUGH __attribute__((fallthrough))
#else
#  define FORCE_INLINE inline
#  define FALLTHROUGH
#endif  // __GNUC__


//------------------------------------------------------------------------

static inline uint32_t rotl32(uint32_t x, int8_t r)
{
        return (x << r) | (x >> (32 - r));
}

//------------------------------------------------------------------------
// Block read - if your platform needs to do endian-swapping or can only
// handle aligned reads, do the conversion here

static FORCE_INLINE uint32_t getblock32 (uint32_t const *p, int i)
{
        return p[i];
}

//------------------------------------------------------------------------
// Finalization mix - force all bits of a hash block to avalanche

static FORCE_INLINE uint32_t fmix32(uint32_t h)
{
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;

        return h;
}

//------------------------------------------------------------------------

unsigned int mptcpd_murmur_hash3(void const *key,
                                 int len,
                                 uint32_t seed)
{
        uint8_t const *const data = (uint8_t const *) key;
        int const nblocks = len / 4;

        uint32_t h1 = seed;

        uint32_t const c1 = 0xcc9e2d51;
        uint32_t const c2 = 0x1b873593;

        //----------
        // body

        uint32_t const *const blocks =
                (uint32_t const *)(data + nblocks * 4);

        for (int i = -nblocks; i; i++) {
                uint32_t k1 = getblock32(blocks, i);

                k1 *= c1;
                k1 = rotl32(k1, 15);
                k1 *= c2;

                h1 ^= k1;
                h1 = rotl32(h1, 13);
                h1 = h1 * 5 + 0xe6546b64;
        }

        //----------
        // tail

        uint8_t const *const tail = (uint8_t const *)(data + nblocks * 4);

        uint32_t k1 = 0;

        switch (len & 3) {
        case 3: k1 ^= tail[2] << 16; FALLTHROUGH;
        case 2: k1 ^= tail[1] << 8;  FALLTHROUGH;
        case 1: k1 ^= tail[0];
                k1 *= c1; k1 = rotl32(k1,15); k1 *= c2; h1 ^= k1;
        };

        //----------
        // finalization

        h1 ^= len;

        h1 = fmix32(h1);

        return h1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
