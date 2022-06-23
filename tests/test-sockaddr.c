// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-sockaddr.c
 *
 * @brief mptcpd sockaddr utilities test.
 *
 * Copyright (c) 2021, 2022, Intel Corporation
 */

#include <ell/log.h>
#include <ell/test.h>

#include <mptcpd/private/sockaddr.h>

#include "test-plugin.h"  // For test sockaddrs

#undef NDEBUG
#include <assert.h>


static void test_network_byte_swap(void const *test_data)
{
        (void) test_data;

        uint16_t const hs = 0x1234;
        uint32_t const hl = 0x040200C0;

#if __BYTE_ORDER == __LITTLE_ENDIAN
        uint16_t const ns = 0x3412;
        uint32_t const nl = 0xC0000204;

        assert(MPTCPD_CONSTANT_HTONS(hs) == ns);
        assert(MPTCPD_CONSTANT_HTONL(hl) == nl);
#else
        assert(MPTCPD_CONSTANT_HTONS(hs) == hs);
        assert(MPTCPD_CONSTANT_HTONL(hl) == hl);
#endif  // __BYTE_ORDER == __LITTLE_ENDIAN
}

static void test_bad_sockaddr_init(void const *test_data)
{
        (void) test_data;

        static in_addr_t const *const bad_addr4 = NULL;
        static struct in6_addr const *const bad_addr6 = NULL;
        static struct sockaddr_storage *const bad_addr = NULL;

        static in_addr_t const *const good_addr4 =
                &test_laddr_1.sin_addr.s_addr;
        static unsigned short const port = 0;

        struct sockaddr_storage good_addr = { .ss_family = AF_UNSPEC };

        // No source in{6}_addrs.
        assert(!mptcpd_sockaddr_storage_init(bad_addr4,
                                             bad_addr6,
                                             port,
                                             &good_addr));

        // No destination addr.
        assert(!mptcpd_sockaddr_storage_init(good_addr4,
                                             bad_addr6,
                                             port,
                                             bad_addr));
}


static void test_sockaddr_in_init(void const *test_data)
{
        (void) test_data;

        static in_addr_t const *const addr4 =
                &test_laddr_1.sin_addr.s_addr;
        static struct in6_addr const *const addr6 = NULL;
        static unsigned short const port = test_laddr_1.sin_port;

        struct sockaddr_storage addr = { .ss_family = AF_UNSPEC };
            
        bool const initialized =
            mptcpd_sockaddr_storage_init(addr4, addr6, port, &addr);

        assert(initialized
               && sockaddr_is_equal((struct sockaddr const *) &test_laddr_1,
                                    (struct sockaddr const *) &addr));
}

static void test_sockaddr_in6_init(void const *test_data)
{
        (void) test_data;

        static in_addr_t const *const addr4 = NULL;
        static struct in6_addr const *const addr6 =
                &test_laddr_2.sin6_addr;
        static unsigned short const port = test_laddr_2.sin6_port;

        struct sockaddr_storage addr = { .ss_family = AF_UNSPEC };
            
        bool const initialized =
            mptcpd_sockaddr_storage_init(addr4, addr6, port, &addr);

        assert(initialized
               && sockaddr_is_equal((struct sockaddr const *) &test_laddr_2,
                                    (struct sockaddr const *) &addr));
}

int main(int argc, char *argv[])
{
        l_log_set_stderr();

        l_test_init(&argc, &argv);

        l_test_add("network byte swap", test_network_byte_swap, NULL);
        l_test_add("bad sockaddr init", test_bad_sockaddr_init, NULL);
        l_test_add("sockaddr_in init",  test_sockaddr_in_init,  NULL);
        l_test_add("sockaddr_in6 init", test_sockaddr_in6_init, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
