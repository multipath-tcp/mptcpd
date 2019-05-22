// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin_two.c
 *
 * @brief MPTCP test plugin.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#undef NDEBUG

#include <assert.h>

#include <ell/plugin.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#include <mptcpd/plugin.h>

#include "test-plugin.h"

// ----------------------------------------------------------------

static struct plugin_call_count call_count;

// ----------------------------------------------------------------

static void plugin_two_new_connection(mptcpd_token_t token,
                                      struct mptcpd_addr const *laddr,
                                      struct mptcpd_addr const *raddr,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(laddr != NULL && raddr != NULL);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_2));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_2));

        ++call_count.new_connection;
}

static void plugin_two_connection_established(
        mptcpd_token_t token,
        struct mptcpd_addr const *laddr,
        struct mptcpd_addr const *raddr,
        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(laddr != NULL && raddr != NULL);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_2));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_2));

        ++call_count.connection_established;
}

static void plugin_two_connection_closed(mptcpd_token_t token,
                                         struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);

        ++call_count.connection_closed;
}

static void plugin_two_new_address(mptcpd_token_t token,
                                   mptcpd_aid_t addr_id,
                                   struct mptcpd_addr const *addr,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(addr_id == test_raddr_id_2);
        assert(mptcpd_addr_is_equal(addr, &test_raddr_2));

        ++call_count.new_address;
}

static void plugin_two_address_removed(mptcpd_token_t token,
                                       mptcpd_aid_t addr_id,
                                       struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(addr_id == test_raddr_id_2);

        ++call_count.address_removed;
}

static void plugin_two_new_subflow(mptcpd_token_t token,
                                   struct mptcpd_addr const *laddr,
                                   struct mptcpd_addr const *raddr,
                                   bool backup,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_2));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_2));
        assert(backup == test_backup_2);

        ++call_count.new_subflow;
}

static void plugin_two_subflow_closed(mptcpd_token_t token,
                                      struct mptcpd_addr const *laddr,
                                      struct mptcpd_addr const *raddr,
                                      bool backup,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_2));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_2));
        assert(backup == test_backup_2);

        ++call_count.subflow_closed;
}

static void plugin_two_subflow_priority(mptcpd_token_t token,
                                        struct mptcpd_addr const *laddr,
                                        struct mptcpd_addr const *raddr,
                                        bool backup,
                                        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_2));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_2));
        assert(backup == test_backup_2);

        ++call_count.subflow_priority;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = plugin_two_new_connection,
        .connection_established = plugin_two_connection_established,
        .connection_closed      = plugin_two_connection_closed,
        .new_address            = plugin_two_new_address,
        .address_removed        = plugin_two_address_removed,
        .new_subflow            = plugin_two_new_subflow,
        .subflow_closed         = plugin_two_subflow_closed,
        .subflow_priority       = plugin_two_subflow_priority
};

static int plugin_two_init(void)
{
        static char const name[] = TEST_PLUGIN;

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize test "
                        "plugin \"" TEST_PLUGIN "\".");

                return -1;
        }

        return 0;
}

static void plugin_two_exit(void)
{
        assert(plugin_call_count_is_sane(&call_count));
        assert(plugin_call_count_is_equal(&call_count, &test_count_2));

        plugin_call_count_reset(&call_count);
}

L_PLUGIN_DEFINE(MPTCPD_PLUGIN_DESC,
                plugin_two,
                "test plugin two",
                VERSION,
                L_PLUGIN_PRIORITY_HIGH,  // unfavorable priority
                plugin_two_init,
                plugin_two_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
