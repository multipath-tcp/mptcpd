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

#include "../src/listener_manager.h"

#include "test-plugin.h"  // For test sockaddrs

#undef NDEBUG
#include <assert.h>


static struct l_hashmap *_lm;
static mptcpd_aid_t const _id = 245;

static void test_create(void const *test_data)
{
        (void) test_data;

        _lm = mptcpd_lm_create();

        assert(_lm != NULL);
}

static void test_listen(void const *test_data)
{
        (void) test_data;

        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_addr   = { .s_addr = htonl(INADDR_LOOPBACK) }
        };

        struct sockaddr const *const sa =
            (struct sockaddr const *) &addr;

        assert(mptcpd_lm_listen(_lm, _id, sa));
}

static void test_close(void const *test_data)
{
        (void) test_data;

        assert(mptcpd_lm_close(_lm, _id));
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

        l_test_add("create listener manager",  test_create,    NULL);
        l_test_add("listen",                   test_listen,    NULL);
        l_test_add("close",                    test_close,     NULL);
        l_test_add("destroy listener manager", test_destroy,   NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
