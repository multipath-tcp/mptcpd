// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-util.c
 *
 * @brief mptcpd test utilities library.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#include <unistd.h>

#include <mptcpd/mptcp_private.h>

#include "test-util.h"

#define MPTCP_SYSCTL_BASE "/proc/sys/net/mptcp/"


char const *tests_get_pm_family_name(void)
{
        static char const upstream[]  = MPTCP_SYSCTL_BASE "enabled";
        static char const mptcp_org[] = MPTCP_SYSCTL_BASE "mptcp_enabled";

        if (access(upstream, R_OK) == 0)
                return MPTCP_PM_NAME;
        else if (access(mptcp_org, R_OK) == 0)
                return MPTCP_GENL_NAME;

        return NULL;  // Not a MPTCP-capable kernel.
}
