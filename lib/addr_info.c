// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file addr_info.c
 *
 * @brief @c mptcpd_addr_info related code.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#include <ell/util.h>

#include <mptcpd/addr_info.h>
#include <mptcpd/sockaddr_private.h>


bool mptcpd_addr_info_init(struct in_addr  const *addr4,
                           struct in6_addr const *addr6,
                           in_port_t       const *port,
                           uint8_t         const *id,
                           uint32_t        const *flags,
                           int32_t         const *index,
                           struct mptcpd_addr_info *info)
{
        if (info == NULL
            || !mptcpd_sockaddr_storage_init(addr4,
                                             addr6,
                                             port ? *port : 0,
                                             &info->addr))
                return false;

        info->id    = (id    ? *id    : 0);
        info->flags = (flags ? *flags : 0);
        info->index = (index ? *index : 0);

        return true;
}

void mptcpd_addr_info_destroy(struct mptcpd_addr_info *info)
{
        l_free(info);
}



/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
