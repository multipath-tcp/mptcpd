// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-murmur-hash.c
 *
 * @brief mptcpd MurmurHash3 test.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <stdint.h>
#include <stddef.h>

#include <ell/ell.h>

#include <mptcpd/private/murmur_hash.h>

#undef NDEBUG
#include <assert.h>


static void test_hash_32(void const *test_data)
{
        (void) test_data;

        /*
          This test is not meant to test the MurmurHash3 algorithm
          itself.  It is only meant to perform a black box test of
          mptcpd_murmur_hash3() since an extensive test suite for the
          MurmurHash3 algorithm already exists upstream.
          See: https://github.com/aappleby/smhasher
        */

        uint32_t const k1 = 0x010200C0;
        uint32_t const k2 = k1 + 1;
        uint8_t  const k3[16] = {
            [0]  = 0x20,
            [1]  = 0x01,
            [2]  = 0x0D,
            [3]  = 0xB8,
            [14] = 0x01,
            [15] = 0x02
        };

        uint32_t const seed = 0xc0ffee;

        unsigned int const hash1 =
            mptcpd_murmur_hash3(&k1, sizeof(k1), seed);
        assert(hash1 != 0);

        unsigned int const hash2 =
            mptcpd_murmur_hash3(&k2, sizeof(k2), seed);
        assert(hash2 != 0);
        assert(hash2 != hash1);

        unsigned int const hash3 =
            mptcpd_murmur_hash3(k3, sizeof(k3), seed);
        assert(hash3 != 0);
        assert(hash3 != hash2);
        assert(hash3 != hash1);

        uint8_t a[31] = { 1, 2, 3 };
        unsigned int const hash4 =
            mptcpd_murmur_hash3(a, sizeof(a), seed);

        l_info("hash1: 0x%x", hash1);
        l_info("hash2: 0x%x", hash2);
        l_info("hash3: 0x%x", hash3);
        l_info("hash4: 0x%x", hash4);
}

int main(int argc, char *argv[])
{
        l_log_set_stderr();

        l_test_init(&argc, &argv);

        l_test_add("test MurmurHash3 (x86_32)", test_hash_32, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
