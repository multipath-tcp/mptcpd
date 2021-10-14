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

int main()
{
        /*
          libmptcpwrap.so should be preloaded when running this
          program, e.g.:

              LD_PRELOAD=libmptcpwrap.so ./mptcpwrap-tester
        */
        int const fd = socket(AF_INET, SOCK_STREAM, 0);
        assert(fd != -1);

        int protocol = 0;
        socklen_t len = sizeof(protocol);

        int const expected_protocol = IPPROTO_TCP + 256;  // MPTCP-ized.

        int const ret = getsockopt(fd,
                                   SOL_SOCKET,
                                   SO_PROTOCOL,
                                   &protocol,
                                   &len);

        close(fd);

        assert(ret == 0
               && protocol == expected_protocol
               && len == sizeof(protocol));

        return 0;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
