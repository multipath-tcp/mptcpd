// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file two.c
 *
 * @brief MPTCP test plugin.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>
#pragma GCC diagnostic pop

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <mptcpd/plugin.h>

#include "test-plugin.h"

#undef NDEBUG
#include <assert.h>


// ----------------------------------------------------------------

static struct plugin_call_count call_count;

static struct sockaddr const *const local_addr =
        (struct sockaddr const *) &test_laddr_2;

static struct sockaddr const *const remote_addr =
        (struct sockaddr const *) &test_raddr_2;

// ----------------------------------------------------------------

static void plugin_two_new_connection(mptcpd_token_t token,
                                      struct sockaddr const *laddr,
                                      struct sockaddr const *raddr,
                                      bool server_side,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(laddr != NULL && raddr != NULL);
        assert(!sockaddr_is_equal(laddr, raddr));
        assert(sockaddr_is_equal(laddr, local_addr));
        assert(sockaddr_is_equal(raddr, remote_addr));
        assert(server_side == test_server_side_2);

        ++call_count.new_connection;
}

static void plugin_two_connection_established(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool server_side,
        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(laddr != NULL && raddr != NULL);
        assert(!sockaddr_is_equal(laddr, raddr));
        assert(sockaddr_is_equal(laddr, local_addr));
        assert(sockaddr_is_equal(raddr, remote_addr));
        assert(server_side == test_server_side_2);

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
                                   struct sockaddr const *addr,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(addr_id == test_raddr_id_2);
        assert(sockaddr_is_equal(addr, remote_addr));

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
                                   struct sockaddr const *laddr,
                                   struct sockaddr const *raddr,
                                   bool backup,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(!sockaddr_is_equal(laddr, raddr));
        assert(sockaddr_is_equal(laddr, local_addr));
        assert(sockaddr_is_equal(raddr, remote_addr));
        assert(backup == test_backup_2);

        ++call_count.new_subflow;
}

static void plugin_two_subflow_closed(mptcpd_token_t token,
                                      struct sockaddr const *laddr,
                                      struct sockaddr const *raddr,
                                      bool backup,
                                      struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(sockaddr_is_equal(laddr, local_addr));
        assert(sockaddr_is_equal(raddr, remote_addr));
        assert(backup == test_backup_2);

        ++call_count.subflow_closed;
}

static void plugin_two_subflow_priority(mptcpd_token_t token,
                                        struct sockaddr const *laddr,
                                        struct sockaddr const *raddr,
                                        bool backup,
                                        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(token == test_token_2);
        assert(!sockaddr_is_equal(laddr, raddr));
        assert(sockaddr_is_equal(laddr, local_addr));
        assert(sockaddr_is_equal(raddr, remote_addr));
        assert(backup == test_backup_2);

        ++call_count.subflow_priority;
}

static void plugin_two_listener_created(struct sockaddr const *laddr,
                                        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(laddr != NULL);
        assert(sockaddr_is_equal(laddr, local_addr));

        ++call_count.listener_created;
}

static void plugin_two_listener_closed(struct sockaddr const *laddr,
                                        struct mptcpd_pm *pm)
{
        (void) pm;

        assert(laddr != NULL);
        assert(sockaddr_is_equal(laddr, local_addr));

        ++call_count.listener_closed;
}

void plugin_two_new_interface(struct mptcpd_interface const *i,
                              struct mptcpd_pm *pm)
{
        (void) i;
        (void) pm;

        ++call_count.new_interface;
}

void plugin_two_update_interface(struct mptcpd_interface const *i,
                                 struct mptcpd_pm *pm)
{
        (void) i;
        (void) pm;

        ++call_count.update_interface;
}

void plugin_two_delete_interface(struct mptcpd_interface const *i,
                                 struct mptcpd_pm *pm)
{
        (void) i;
        (void) pm;

        ++call_count.delete_interface;
}

void plugin_two_new_local_address(struct mptcpd_interface const *i,
                                  struct sockaddr const *sa,
                                  struct mptcpd_pm *pm)
{
        (void) i;
        (void) sa;
        (void) pm;

        ++call_count.new_local_address;
}

void plugin_two_delete_local_address(struct mptcpd_interface const *i,
                                     struct sockaddr const *sa,
                                     struct mptcpd_pm *pm)
{
        (void) i;
        (void) sa;
        (void) pm;

        ++call_count.delete_local_address;
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection         = plugin_two_new_connection,
        .connection_established = plugin_two_connection_established,
        .connection_closed      = plugin_two_connection_closed,
        .new_address            = plugin_two_new_address,
        .address_removed        = plugin_two_address_removed,
        .new_subflow            = plugin_two_new_subflow,
        .subflow_closed         = plugin_two_subflow_closed,
        .subflow_priority       = plugin_two_subflow_priority,
        .listener_created       = plugin_two_listener_created,
        .listener_closed        = plugin_two_listener_closed,
        .new_interface          = plugin_two_new_interface,
        .update_interface       = plugin_two_update_interface,
        .delete_interface       = plugin_two_delete_interface,
        .new_local_address      = plugin_two_new_local_address,
        .delete_local_address   = plugin_two_delete_local_address
};

static int plugin_two_init(struct mptcpd_pm *pm)
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

static void plugin_two_exit(struct mptcpd_pm *pm)
{
        (void) pm;

        assert(call_count_is_sane(&call_count));
        assert(call_count_is_equal(&call_count, &test_count_2));

        call_count_reset(&call_count);
}

MPTCPD_PLUGIN_DEFINE(plugin_two,
                     "test plugin two",
                     MPTCPD_PLUGIN_PRIORITY_LOW,  // unfavorable priority
                     plugin_two_init,
                     plugin_two_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
