// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpd/private/mptcp_org.h
 *
 * @brief Convenience wrapper around multipath-tcp.org <linux/mptcp.h>.
 *
 * Support for the multipath-tcp.org kernel <linux/mptcp.h> header.
 *
 * Copyright (c) 2020-2021, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_MPTCP_ORG_H
#define MPTCPD_PRIVATE_MPTCP_ORG_H

#ifdef HAVE_CONFIG_H
# include "mptcpd/config-private.h"
#endif

#ifdef HAVE_LINUX_MPTCP_H_MPTCP_ORG
# include <linux/mptcp.h>
#else
/*
  Platform does not have multipath-tcp.org MPTCP path mangement
  generic netlink API so include local copy of the multipath-tcp-org
  <linux/mptcp.h> header.
*/
#  include "linux/mptcp_org.h"
#endif // HAVE_LINUX_MPTCP_H_MPTCP_ORG

#endif // MPTCPD_PRIVATE_MPTCP_ORG_H
