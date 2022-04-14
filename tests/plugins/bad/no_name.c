// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file no_name.c
 *
 * @brief MPTCP test plugin missing a name.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <mptcpd/plugin.h>


static int plugin_no_name_init(struct mptcpd_pm *pm)
{
        (void) pm;

        return 0;
}

static void plugin_no_name_exit(struct mptcpd_pm *pm)
{
        (void) pm;
}

/*
  Do not use the MPTCPD_PLUGIN_DEFINE() macro so that we can force the
  plugin name to be NULL.
 */
extern struct mptcpd_plugin_desc const MPTCPD_PLUGIN_SYM
        __attribute__((visibility("default")));

struct mptcpd_plugin_desc const MPTCPD_PLUGIN_SYM = {
        NULL,  // plugin name
        "plugin with no name",
        0, /* version */
        MPTCPD_PLUGIN_PRIORITY_DEFAULT,
        plugin_no_name_init,
        plugin_no_name_exit
};


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
