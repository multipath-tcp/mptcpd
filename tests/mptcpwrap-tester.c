// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpwrap-tester.c
 *
 * @brief Test wrapped socket() call for MPTCP protocol injection.
 *
 * Copyright (c) 2021, Intel Corporation
 */

#undef NDEBUG
#include <assert.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <stdbool.h>


struct socket_data
{
        int  domain;
        int  type;
        int  protocol;
        bool expect_mptcp;
};

static bool verify_protocol(int fd, bool expect_mptcp)
{
        int protocol = 0;
        socklen_t len = sizeof(protocol);

        int const ret = getsockopt(fd,
                                   SOL_SOCKET,
                                   SO_PROTOCOL,
                                   &protocol,
                                   &len);

        /*
          The MPTCP-ized protocol injected by the hijacked socket()
          call, i.e. IPPROTO_MPTCP.
        */
        static int const mptcp_protocol = IPPROTO_TCP + 256;

        return ret == 0
                && (expect_mptcp ? protocol == mptcp_protocol : true);
}

static void test_socket_data(struct socket_data const *data)
{
        /*
          libmptcpwrap.so should be preloaded when running this
          program, e.g.:

          LD_PRELOAD=libmptcpwrap.so ./mptcpwrap-tester
        */
        int const fd = socket(data->domain, data->type, data->protocol);
        assert(fd != -1);

        bool const verified = verify_protocol(fd, data->expect_mptcp);

        close(fd);

        assert(verified);
}

int main()
{
        /*
          MPTCP is only injected when using the SOCK_STREAM socket
          type and a protocol value of 0 or IPPROTO_TCP.
        */
        static struct socket_data const data[] = {
                { AF_LOCAL, SOCK_STREAM, 0,            false },
                { AF_INET,  SOCK_STREAM, IPPROTO_SCTP, false },
                { AF_INET,  SOCK_DGRAM,  0,            false },
                { AF_INET,  SOCK_STREAM, 0,            true  },
                { AF_INET6, SOCK_STREAM, 0,            true  },
                { AF_INET,  SOCK_STREAM, IPPROTO_TCP,  true  },
                { AF_INET6, SOCK_STREAM, IPPROTO_TCP,  true  }
        };

        static struct socket_data const *const end =
                data + (sizeof(data) / sizeof(data[0]));

        for (struct socket_data const *d = data; d != end; ++d)
                test_socket_data(d);

        return 0;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
