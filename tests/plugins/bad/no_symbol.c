// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file no_symbol.c
 *
 * @brief MPTCP test plugin missing the expected plugin symbol.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <mptcpd/plugin.h>


static int plugin_no_symbol_init(struct mptcpd_pm *pm)
{
        (void) pm;

        return 0;
}

static void plugin_no_symbol_exit(struct mptcpd_pm *pm)
{
        (void) pm;
}

/*
  Do not use the MPTCPD_PLUGIN_DEFINE() macro so that we can force the
  expected mptcpd plugin symbol, i.e. MPTCPD_PLUGIN_SYM, to be
  missing.
 */
extern struct mptcpd_plugin_desc const MPTCPD_BAD_PLUGIN_SYM
        __attribute__((visibility("default")));

struct mptcpd_plugin_desc const MPTCPD_BAD_PLUGIN_SYM = {
        NULL,  // plugin name
        "plugin with no name",
        0, /* version */
        MPTCPD_PLUGIN_PRIORITY_DEFAULT,
        plugin_no_symbol_init,
        plugin_no_symbol_exit
};


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
