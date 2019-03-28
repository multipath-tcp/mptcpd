// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin_noop.c
 *
 * @brief MPTCP test plugin.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#include <ell/plugin.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#include <mptcpd/plugin.h>


static void plugin_noop_new_connection(mptcpd_cid_t connection_id,
                                       struct mptcpd_addr const *laddr,
                                       struct mptcpd_addr const *raddr,
                                       bool backup,
                                       struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) laddr;
        (void) raddr;
        (void) backup;
        (void) pm;
}

static void plugin_noop_new_address(mptcpd_cid_t connection_id,
                                    mptcpd_aid_t addr_id,
                                    struct mptcpd_addr const *addr,
                                    struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) addr_id;
        (void) addr;
        (void) pm;
}

static void plugin_noop_new_subflow(mptcpd_cid_t connection_id,
                                    mptcpd_aid_t laddr_id,
                                    struct mptcpd_addr const *laddr,
                                    mptcpd_aid_t raddr_id,
                                    struct mptcpd_addr const *raddr,
                                    struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) laddr_id;
        (void) laddr;
        (void) raddr_id;
        (void) raddr;
        (void) pm;
}

static void plugin_noop_subflow_closed(mptcpd_cid_t connection_id,
                                       struct mptcpd_addr const *laddr,
                                       struct mptcpd_addr const *raddr,
                                       struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) laddr;
        (void) raddr;
        (void) pm;
}

static void plugin_noop_connection_closed(mptcpd_cid_t connection_id,
                                          struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) pm;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection    = plugin_noop_new_connection,
        .new_address       = plugin_noop_new_address,
        .new_subflow       = plugin_noop_new_subflow,
        .subflow_closed    = plugin_noop_subflow_closed,
        .connection_closed = plugin_noop_connection_closed
};

static int plugin_noop_init(void)
{
        static char const name[] = TEST_PLUGIN;

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize test "
                        "plugin \"" TEST_PLUGIN "\".");

                return -1;
        }

        return 0;
}

static void plugin_noop_exit(void)
{
}

L_PLUGIN_DEFINE(MPTCPD_PLUGIN_DESC,
                plugin_noop,
                "test plugin noop",
                VERSION,
                L_PLUGIN_PRIORITY_DEFAULT,
                plugin_noop_init,
                plugin_noop_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
