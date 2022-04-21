// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-addr-info.c
 *
 * @brief mptcpd_addr_info related functions test.
 *
 * Copyright (c) 2021, Intel Corporation
 */

#include <ell/log.h>
#include <ell/test.h>

#include <mptcpd/addr_info.h>
#include <mptcpd/private/addr_info.h>
#include <mptcpd/private/sockaddr.h>

#undef NDEBUG
#include <assert.h>


static void test_bad_addr_info(void const *test_data)
{
        (void) test_data;

        // Check underlying sockaddr.
        struct sockaddr const *sa = mptcpd_addr_info_get_addr(NULL);
        assert(sa == NULL);

        // Check MPTCP address ID.
        mptcpd_aid_t const id = mptcpd_addr_info_get_id(NULL);
        assert(id == 0);

        // Check mptcpd flags.
        mptcpd_flags_t const flags = mptcpd_addr_info_get_flags(NULL);
        assert(flags == 0);

        // Check network interface index.
        int const index = mptcpd_addr_info_get_index(NULL);
        assert(index == -1);
}

static void test_addr_info(void const *test_data)
{
        (void) test_data;

        static in_addr_t const addr = 0x010200C0;  // 192.0.2.1
        static in_port_t const port = 12;
        
        struct mptcpd_addr_info info = {
                .id    = 5,
                .flags = 10,
                .index = 2
        };

        assert(mptcpd_sockaddr_storage_init(addr,
                                            NULL,
                                            port,
                                            &info.addr));


        // Check underlying sockaddr.
        struct sockaddr const *sa = mptcpd_addr_info_get_addr(&info);
        assert(sa != NULL && sa->sa_family == AF_INET);

        struct sockaddr_in const *sai = (struct sockaddr_in const *) sa;
        assert(sai->sin_addr.s_addr == addr);
        assert(sai->sin_port == port);

        // Check MPTCP address ID.
        mptcpd_aid_t const id = mptcpd_addr_info_get_id(&info);
        assert(id == info.id);

        // Check mptcpd flags.
        mptcpd_flags_t const flags = mptcpd_addr_info_get_flags(&info);
        assert(flags == info.flags);

        // Check network interface index.
        int const index = mptcpd_addr_info_get_index(&info);
        assert(index == info.index);
}


int main(int argc, char *argv[])
{
        l_log_set_stderr();

        l_test_init(&argc, &argv);

        l_test_add("bad  addr_info", test_bad_addr_info, NULL);
        l_test_add("good addr_info", test_addr_info,  NULL);

        return l_test_run();
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
