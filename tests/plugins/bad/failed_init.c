// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file failed_init.c
 *
 * @brief MPTCP test plugin with @c init function that fails.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <mptcpd/plugin.h>


static int plugin_failed_init(struct mptcpd_pm *pm)
{
        (void) pm;

        return -1;
}

MPTCPD_PLUGIN_DEFINE(plugin_failed_init,
                     "test plugin for failed init",
                     MPTCPD_PLUGIN_PRIORITY_DEFAULT,
                     plugin_failed_init,
                     NULL)   // exit



/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
