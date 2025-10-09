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

/*
  Platform might have upstream MPTCP path mangement generic netlink API, but
  always include a local copy which is synced with the supported features.
*/
#include "linux/mptcp_upstream.h"

#endif // MPTCPD_PRIVATE_MPTCP_UPSTREAM_H
