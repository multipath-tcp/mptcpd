// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-listener-manager.c
 *
 * @brief mptcpd listener manager test.
 *
 * Copyright (c) 2022, Intel Corporation
 */


#include <arpa/inet.h>

#include <ell/log.h>
#include <ell/test.h>
#include <ell/hashmap.h>

#include <mptcpd/listener_manager.h>

#include "test-plugin.h"  // For test sockaddrs

#undef NDEBUG
#include <assert.h>


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

        l_test_init(&argc, &argv);

        /*
          Listen on the loopback address since we need an address
          backed by a network interface so that underlying bind() call
          can succeed.
        */
        struct sockaddr_in addr4 = {
            .sin_family = AF_INET,
            .sin_addr   = { .s_addr = htonl(INADDR_LOOPBACK) }
        };

        struct sockaddr const *const sa4 =
            (struct sockaddr const *) &addr4;

        /*
          Listen on the loopback address since we need an address
          backed by a network interface so that underlying bind() call
          can succeed.
        */
        struct sockaddr_in6 addr6 = {
            .sin6_family = AF_INET6
        };

        addr6.sin6_addr = in6addr_loopback;

        struct sockaddr const *const sa6 =
            (struct sockaddr const *) &addr6;

        l_test_add("create listener manager",  test_create,  NULL);
        l_test_add("listen - IPv4 - FIRST",    test_listen,  sa4);
        l_test_add("listen - IPv6",            test_listen,  sa6);
        l_test_add("listen - IPv4 - SECOND",   test_listen,  sa4);
        l_test_add("close  - IPv4",            test_close,   sa4);
        l_test_add("close  - IPv6",            test_close,   sa6);
        l_test_add("destroy listener manager", test_destroy, NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
