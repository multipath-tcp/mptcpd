// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file addr_info.c
 *
 * @brief @c mptcpd_addr_info related functions.
 *
 * Copyright (c) 2021, Intel Corporation
 */

#include <mptcpd/addr_info.h>
#include <mptcpd/private/addr_info.h>


struct sockaddr const *
mptcpd_addr_info_get_addr(struct mptcpd_addr_info const *info)
{
        return (struct sockaddr const *) &info->addr;
}

mptcpd_aid_t mptcpd_addr_info_get_id(struct mptcpd_addr_info const *info)
{
        return info->id;
}

mptcpd_flags_t
mptcpd_addr_info_get_flags(struct mptcpd_addr_info const *info)
{
        return info->flags;
}

int mptcpd_addr_info_get_index(struct mptcpd_addr_info const *info)
{
        return info->index;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
