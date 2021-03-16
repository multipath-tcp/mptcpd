// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpd/private/mptcp_upstream.h
 *
 * @brief mptcpd convenience wrapper around upstream <linux/mptcp.h>.
 *
 * Support for the upstream kernel <linux/mptcp.h> header.
 *
 * Copyright (c) 2020-2021, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_MPTCP_UPSTREAM_H
#define MPTCPD_PRIVATE_MPTCP_UPSTREAM_H

#ifdef HAVE_CONFIG_H
# include "mptcpd/private/config.h"
#endif

#if defined(HAVE_LINUX_MPTCP_H_UPSTREAM) \
    && defined(HAVE_LINUX_MPTCP_H_UPSTREAM_EVENTS)
# include <linux/mptcp.h>
#else
/*
  Platform does not have upstream MPTCP path mangement generic netlink
  API so include local copy of the upstream <linux/mptcp.h> header.
*/
# include "linux/mptcp_upstream.h"
#endif // HAVE_LINUX_MPTCP_H_UPSTREAM

#endif // MPTCPD_PRIVATE_MPTCP_UPSTREAM_H
