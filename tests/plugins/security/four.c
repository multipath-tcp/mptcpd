// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file four.c
 *
 * @brief MPTCP test plugin.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#include <ell/ell.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <mptcpd/plugin.h>

#include "test-plugin.h"

#undef NDEBUG
#include <assert.h>


// ----------------------------------------------------------------

static struct plugin_call_count call_count;

// ----------------------------------------------------------------

static void plugin_four_new_connection(mptcpd_token_t token,
                                       struct sockaddr const *laddr,
                                       struct sockaddr const *raddr,
                                       bool server_side,
                                       struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) server_side;
        (void) pm;

        ++call_count.new_connection;
}

static void plugin_four_connection_established(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool server_side,
        struct mptcpd_pm *pm)
{
        (void) token;
        (void) laddr;
        (void) raddr;
        (void) server_side;
        (void) pm;

        ++call_count.connection_established;
}

static void plugin_four_connection_closed(mptcpd_token_t token,
                                           struct mptcpd_pm *pm)
{
        (void) token;
        (void) pm;

        ++call_count.connection_closed;
}

static void plugin_four_new_address(mptcpd_token_t token,
                                     mptcpd_aid_t addr_id,
                                     struct sockaddr const *addr,
                                     struct mptcpd_pm *pm)
{
        (void) token;
        (void) addr_id;
        (void) addr;
        (void) pm;

        ++call_count.new_address;
}

static void plugin_four_address_removed(mptcpd_token_t token,
                                         mptcpd_aid_t addr_id,
                                         struct mptcpd_pm *pm)
{
        (void) token;
        (void) addr_id;
        (void) pm;

        ++call_count.address_removed;
}

static void plugin_four_new_subflow(mptcpd_token_t token,
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

        ++call_count.new_subflow;
}

static void plugin_four_subflow_closed(mptcpd_token_t token,
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

        ++call_count.subflow_closed;
}

static void plugin_four_subflow_priority(mptcpd_token_t token,
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

        ++call_count.subflow_priority;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = plugin_four_new_connection,
        .connection_established = plugin_four_connection_established,
        .connection_closed      = plugin_four_connection_closed,
        .new_address            = plugin_four_new_address,
        .address_removed        = plugin_four_address_removed,
        .new_subflow            = plugin_four_new_subflow,
        .subflow_closed         = plugin_four_subflow_closed,
        .subflow_priority       = plugin_four_subflow_priority,
};

static int plugin_four_init(struct mptcpd_pm *pm)
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

static void plugin_four_exit(struct mptcpd_pm *pm)
{
        (void) pm;

        assert(call_count_is_sane(&call_count));
        assert(call_count_is_equal(&call_count, &test_count_4));

        call_count_reset(&call_count);
}

MPTCPD_PLUGIN_DEFINE(plugin_four,
                     "test plugin four",
                     MPTCPD_PLUGIN_PRIORITY_LOW,  // Unfavorable priority.
                     plugin_four_init,
                     plugin_four_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
