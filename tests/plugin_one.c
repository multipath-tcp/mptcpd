// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin_one.c
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

static void plugin_one_new_connection(mptcpd_token_t token,
                                      struct mptcpd_addr const *laddr,
                                      struct mptcpd_addr const *raddr,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(laddr != NULL && raddr != NULL);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_1));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_1));

        ++call_count.new_connection;
}

static void plugin_one_connection_established(
        mptcpd_token_t token,
        struct mptcpd_addr const *laddr,
        struct mptcpd_addr const *raddr,
        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(laddr != NULL && raddr != NULL);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_1));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_1));

        ++call_count.connection_established;
}

static void plugin_one_connection_closed(mptcpd_token_t token,
                                         struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);

        ++call_count.connection_closed;
}

static void plugin_one_new_address(mptcpd_token_t token,
                                   mptcpd_aid_t addr_id,
                                   struct mptcpd_addr const *addr,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(addr_id == test_raddr_id_1);
        assert(mptcpd_addr_is_equal(addr, &test_raddr_1));

        ++call_count.new_address;
}

static void plugin_one_address_removed(mptcpd_token_t token,
                                       mptcpd_aid_t addr_id,
                                       struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(addr_id == test_raddr_id_1);

        ++call_count.new_address;
}

static void plugin_one_new_subflow(mptcpd_token_t token,
                                   struct mptcpd_addr const *laddr,
                                   struct mptcpd_addr const *raddr,
                                   bool backup,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(!mptcpd_addr_is_equal(laddr, raddr));
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_1));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_1));
        assert(backup == test_backup_1);

        ++call_count.new_subflow;
}

static void plugin_one_subflow_closed(mptcpd_token_t token,
                                      struct mptcpd_addr const *laddr,
                                      struct mptcpd_addr const *raddr,
                                      bool backup,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_1));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_1));
        assert(backup == test_backup_1);

        ++call_count.subflow_closed;
}

static void plugin_one_subflow_priority(mptcpd_token_t token,
                                        struct mptcpd_addr const *laddr,
                                        struct mptcpd_addr const *raddr,
                                        bool backup,
                                        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_1);
        assert(mptcpd_addr_is_equal(laddr, &test_laddr_1));
        assert(mptcpd_addr_is_equal(raddr, &test_raddr_1));
        assert(backup == test_backup_1);

        ++call_count.subflow_priority;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = plugin_one_new_connection,
        .connection_established = plugin_one_connection_established,
        .connection_closed      = plugin_one_connection_closed,
        .new_address            = plugin_one_new_address,
        .address_removed        = plugin_one_address_removed,
        .new_subflow            = plugin_one_new_subflow,
        .subflow_closed         = plugin_one_subflow_closed,
        .subflow_priority       = plugin_one_subflow_priority
};

static int plugin_one_init(void)
{
        static char const name[] = TEST_PLUGIN;

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize test "
                        "plugin \"" TEST_PLUGIN "\".");

                return -1;
        }

        return 0;
}

static void plugin_one_exit(void)
{
        /*
          This plugin should be called twice, once with the plugin name
          explicitly specified, and once as the default plugin with no
          name specified.
        */
        struct plugin_call_count const count = {
                .new_connection    = test_count_1.new_connection    * 2,
                .connection_established =
                        test_count_1.connection_established * 2,
                .connection_closed = test_count_1.connection_closed * 2,
                .new_address       = test_count_1.new_address       * 2,
                .address_removed   = test_count_1.address_removed   * 2,
                .new_subflow       = test_count_1.new_subflow       * 2,
                .subflow_closed    = test_count_1.subflow_closed    * 2,
                .subflow_priority  = test_count_1.subflow_priority  * 2,
        };

        assert(plugin_call_count_is_sane(&call_count));
        assert(plugin_call_count_is_equal(&call_count, &count));

        plugin_call_count_reset(&call_count);
}

L_PLUGIN_DEFINE(MPTCPD_PLUGIN_DESC,
                plugin_one,
                "test plugin one",
                VERSION,
                L_PLUGIN_PRIORITY_LOW,  // favorable priority
                plugin_one_init,
                plugin_one_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
