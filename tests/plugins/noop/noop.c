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


static void plugin_noop_new_connection(mptcpd_token_t token,
                                       struct sockaddr const *laddr,
                                       struct sockaddr const *raddr,
                                       struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) pm;
}

static void plugin_noop_connection_established(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) pm;
}

static void plugin_noop_connection_closed(mptcpd_token_t token,
                                          struct mptcpd_pm *pm)
{
        (void) token;
        (void) pm;
}

static void plugin_noop_new_address(mptcpd_token_t token,
                                    mptcpd_aid_t addr_id,
                                    struct sockaddr const *addr,
                                    struct mptcpd_pm *pm)
{
        (void) token;
        (void) addr_id;
        (void) addr;
        (void) pm;
}

static void plugin_noop_address_removed(mptcpd_token_t token,
                                         mptcpd_aid_t addr_id,
                                         struct mptcpd_pm *pm)
{
        (void) token;
        (void) addr_id;
        (void) pm;
}

static void plugin_noop_new_subflow(mptcpd_token_t token,
                                    struct sockaddr const *laddr,
                                    struct sockaddr const *raddr,
                                    bool backup,
                                    struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) backup;
        (void) pm;
}

static void plugin_noop_subflow_closed(mptcpd_token_t token,
                                       struct sockaddr const *laddr,
                                       struct sockaddr const *raddr,
                                       bool backup,
                                       struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) backup;
        (void) pm;
}

static void plugin_noop_subflow_priority(mptcpd_token_t token,
                                         struct sockaddr const *laddr,
                                         struct sockaddr const *raddr,
                                         bool backup,
                                         struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) backup;
        (void) pm;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = plugin_noop_new_connection,
        .connection_established = plugin_noop_connection_established,
        .connection_closed      = plugin_noop_connection_closed,
        .new_address            = plugin_noop_new_address,
        .address_removed        = plugin_noop_address_removed,
        .new_subflow            = plugin_noop_new_subflow,
        .subflow_closed         = plugin_noop_subflow_closed,
        .subflow_priority       = plugin_noop_subflow_priority
};

static int plugin_noop_init(struct mptcpd_pm *pm)
{
        (void) pm;

        static char const name[] = TEST_PLUGIN;

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize test "
                        "plugin \"" TEST_PLUGIN "\".");

                return -1;
        }

        return 0;
}

static void plugin_noop_exit(struct mptcpd_pm *pm)
{
        (void) pm;
}

MPTCPD_PLUGIN_DEFINE(plugin_noop,
                     "test plugin noop",
                     MPTCPD_PLUGIN_PRIORITY_DEFAULT,
                     plugin_noop_init,
                     plugin_noop_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
