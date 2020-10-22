// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file commands.h
 *
 * @brief mptcpd generic netlink command utilities.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifndef MPTCPD_COMMANDS_H
#define MPTCPD_COMMANDS_H

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#include <stdbool.h>
#include <stddef.h>   // For NULL.
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/netlink.h>  // For NLA_* macros.

#define MPTCPD_NLA_ALIGN(v) NLA_HDRLEN + NLA_ALIGN(sizeof(v))
#define MPTCPD_NLA_ALIGN_OPT(v) (v) == 0 ? 0 : (MPTCPD_NLA_ALIGN(v))


#ifdef __cplusplus
extern "C" {
#endif

inline bool mptcpd_is_inet_family(struct sockaddr const *addr)
{
        return addr->sa_family == AF_INET || addr->sa_family == AF_INET6;
}

inline size_t mptcpd_get_addr_size(struct sockaddr const *addr)
{
        assert(mptcpd_is_inet_family(addr));

        return addr->sa_family == AF_INET
                ? sizeof(struct in_addr)
                : sizeof(struct in6_addr);
}

inline uint16_t mptcpd_get_addr_family(struct sockaddr const *addr)
{
        sa_family_t const family = (addr == NULL ? 0 : addr->sa_family);

        return family;
}

struct mptcpd_pm_cmd_ops const *mptcpd_get_upstream_cmd_ops(void);
struct mptcpd_pm_cmd_ops const *mptcpd_get_mptcp_org_cmd_ops(void);

#ifdef __cplusplus
}
#endif


#endif /* MPTCPD_COMMANDS_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
