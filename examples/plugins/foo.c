// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file foo.c
 *
 * @brief Sample mptcpd plugin.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#include <ell/ell.h>
#include <mptcpd/network_monitor.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>

#define PLUGIN_NAME foo

static void foo_new_connection(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               struct mptcpd_pm *pm)
{
    // Handle creation of new MPTCP connection.
}

static void foo_connection_established(
    mptcpd_token_t token,
    struct sockaddr const *laddr,
    struct sockaddr const *raddr,
    struct mptcpd_pm *pm)
{
    // Handle establishment of new MPTCP connection.
}

static void foo_connection_closed(mptcpd_token_t token,
                                  struct mptcpd_pm *pm)
{
    // Handle MPTCP connection closure.
}

static void foo_new_address(mptcpd_token_t token,
                            mptcpd_aid_t addr_id,
                            struct sockaddr const *addr,
                            struct mptcpd_pm *pm)
{
    // Handle address advertised by MPTCP capable peer.
}

static void foo_address_removed(mptcpd_token_t token,
                                mptcpd_aid_t addr_id,
                                struct mptcpd_pm *pm)
{
    // Handle address no longer advertised by MPTCP capable peer.
}

static void foo_new_subflow(mptcpd_token_t token,
                            struct sockaddr const *laddr,
                            struct sockaddr const *raddr,
                            bool backup,
                            struct mptcpd_pm *pm)
{
    // Handle new subflow added to the MPTCP connection.
}

static void foo_subflow_closed(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               bool backup,
                               struct mptcpd_pm *pm)
{
    // Handle MPTCP subflow closure.
}

static void foo_subflow_priority(mptcpd_token_t token,
                                 struct sockaddr const *laddr,
                                 struct sockaddr const *raddr,
                                 bool backup,
                                 struct mptcpd_pm *pm)
{
    // Handle change in MPTCP subflow priority.
}

static struct mptcpd_plugin_ops const pm_ops = {
    .new_connection         = foo_new_connection,
    .connection_established = foo_connection_established,
    .connection_closed      = foo_connection_closed,
    .new_address            = foo_new_address,
    .address_removed        = foo_address_removed,
    .new_subflow            = foo_new_subflow,
    .subflow_closed         = foo_subflow_closed,
    .subflow_priority       = foo_subflow_priority
};

static int foo_init(void)
{
    static char const name[] = L_STRINGIFY(PLUGIN_NAME);

    if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
        l_error("Failed to initialize plugin '%s'.", name);

        return -1;
    }

    return 0;
}

static void foo_exit(void)
{
}

L_PLUGIN_DEFINE(MPTCPD_PLUGIN_DESC,
                PLUGIN_NAME,
                "foo path management plugin",
                VERSION,
                L_PLUGIN_PRIORITY_DEFAULT,
                foo_init,
                foo_exit)
