// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-listener-manager.c
 *
 * @brief mptcpd listener manager test.
 *
 * Copyright (c) 2022, Intel Corporation
 */


#include <arpa/inet.h>
#include <netinet/in.h>

#include <ell/util.h>
#include <ell/log.h>
#include <ell/test.h>
#include <ell/hashmap.h>

#include <mptcpd/private/listener_manager.h>
#include <mptcpd/listener_manager.h>

#include "test-plugin.h"  // For test sockaddrs

#undef NDEBUG
#include <assert.h>


struct ipv4_listen_case
{
        struct sockaddr_in const addr;
        char const *const desc;
};

struct ipv6_listen_case
{
        struct sockaddr_in6 const addr;
        char const *const desc;
};

/**
 * @brief Initialize test case to listen on IPv4 loopback address.
 *
 * @param[in] port IP port to listen on.
 */
#define INIT_IPV4_TEST_CASE(port)                                       \
        {                                                               \
                .addr = {                                               \
                        .sin_family = AF_INET,                          \
                        .sin_port   = htons(port),                      \
                        .sin_addr   = {                                 \
                                .s_addr = htonl(INADDR_LOOPBACK)        \
                        }                                               \
                },                                                      \
                .desc = "listen - IPv4 on port " L_STRINGIFY(port)      \
        }

/**
 * @brief Initialize test case to listen on IPv6 loopback address.
 *
 * @param[in] port IP port to listen on.
 */
#define INIT_IPV6_TEST_CASE(port)                                       \
        {                                                               \
                .addr = {                                               \
                        .sin6_family = AF_INET6,                        \
                        .sin6_port   = htons(port),                     \
                        .sin6_addr   = { .s6_addr = { [15] = 0x01 } }   \
                },                                                      \
                .desc = "listen - IPv6 on port " L_STRINGIFY(port)      \
        }

// -----------------------------------------------------------------------

static struct mptcpd_lm *_lm;

static void test_create(void const *test_data)
{
        (void) test_data;

        _lm = mptcpd_lm_create();

        assert(_lm != NULL);
}

static void test_listen(void const *test_data)
{
        struct sockaddr const *const sa = test_data;

        assert(mptcpd_lm_listen(_lm, sa));
}

static void test_close(void const *test_data)
{
        struct sockaddr const *const sa = test_data;

        assert(mptcpd_lm_close(_lm, sa));
}

static void test_destroy(void const *test_data)
{
        (void) test_data;

        mptcpd_lm_destroy(_lm);
}

int main(int argc, char *argv[])
{
        l_log_set_stderr();
        //l_debug_enable("*");

        l_test_init(&argc, &argv);

        /*
          Listen on the IPv4 and IPv6 loopback addresses since we need
          an address backed by a network interface so that the
          underlying bind() call can succeed.
        */
        struct ipv4_listen_case const ipv4_cases[] = {
                INIT_IPV4_TEST_CASE(0x3456),
                INIT_IPV4_TEST_CASE(0x3457),
                INIT_IPV4_TEST_CASE(0x3456),  // Same port as above.
                INIT_IPV4_TEST_CASE(0)
        };

        struct ipv6_listen_case const ipv6_cases[] = {
                INIT_IPV6_TEST_CASE(0x4567),
                INIT_IPV6_TEST_CASE(0x4578),
                INIT_IPV6_TEST_CASE(0x4567),  // Same port as above.
                INIT_IPV6_TEST_CASE(0)
        };


        l_test_add("create lm", test_create, NULL);

        for (size_t i = 0; i < L_ARRAY_SIZE(ipv4_cases); ++i) {
                char const *const desc = ipv4_cases[i].desc;
                struct sockaddr const *const sa =
                        (struct sockaddr const *) &ipv4_cases[i].addr;

                l_test_add(desc, test_listen, sa);
        }

        for (size_t i = 0; i < L_ARRAY_SIZE(ipv6_cases); ++i) {
                char const *const desc = ipv6_cases[i].desc;
                struct sockaddr const *const sa =
                        (struct sockaddr const *) &ipv6_cases[i].addr;

                l_test_add(desc, test_listen, sa);
        }

        l_test_add("close  - IPv4",
                   test_close,
                   (struct sockaddr const *) &ipv4_cases[0]);
        l_test_add("close  - IPv6",
                   test_close,
                   (struct sockaddr const *) &ipv6_cases[0]);

        l_test_add("destroy lm",    test_destroy, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
