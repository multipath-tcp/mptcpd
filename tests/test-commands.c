// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-commands.c
 *
 * @brief mptcpd commands API test.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <net/if.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/main.h>
#include <ell/idle.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/netlink.h>
#include <ell/rtnl.h>
#include <ell/test.h>
#pragma GCC diagnostic pop

// Internal Headers
// -----------------
#include <mptcpd/private/configuration.h>
#include "../src/path_manager.h"
#include "../src/commands.h"
// -----------------

#include "test-plugin.h"
#include "test-util.h"

#include <mptcpd/path_manager.h>
#include <mptcpd/addr_info.h>
#include <mptcpd/id_manager.h>

#undef NDEBUG
#include <assert.h>

// -------------------------------------------------------------------

struct test_info
{
        struct l_netlink *const rtnl;

        // Address used for kernel add_addr and dump_addr calls.
        struct sockaddr const *const addr;

        // Network interface on which to bind the test address.
        int const ifindex;

        // CIDR prefix
        uint8_t const prefix;

        // String form of the addr.
        char const *const ip;

        // Mptcpd configuration.
        struct mptcpd_config *config;

        // Mptcpd path manager object.
        struct mptcpd_pm *pm;

        // ID used for kernel add_addr and dump_addr calls.
        mptcpd_aid_t id;

        // Number of times dump_addrs call was completed.
        int dump_addrs_complete_count;

        // Were the tests called?
        bool tests_called;
};

// -------------------------------------------------------------------

static struct sockaddr const *const laddr1 =
        (struct sockaddr const *) &test_laddr_1;

static struct sockaddr const *const laddr2 =
        (struct sockaddr const *) &test_laddr_2;

static struct sockaddr const *const raddr1 =
        (struct sockaddr const *) &test_raddr_1;

static struct sockaddr const *const raddr2 =
        (struct sockaddr const *) &test_raddr_2;

// -------------------------------------------------------------------

static uint32_t const max_addrs = 3;
static uint32_t const max_subflows = 5;

static struct mptcpd_limit const _limits[] = {
        {
                .type  = MPTCPD_LIMIT_RCV_ADD_ADDRS,
                .limit = max_addrs
        },
        {
                .type  = MPTCPD_LIMIT_SUBFLOWS,
                .limit = max_subflows
        }
};

// -------------------------------------------------------------------

static void get_addr_callback(struct mptcpd_addr_info const *info,
                              void *user_data)
{
        struct test_info *const tinfo = (struct test_info *) user_data;

        /**
         * @bug We could have a resource leak in the kernel here if
         *      the below assert()s are triggered since addresses
         *      previously added through @c mptcpd_kpm_add_addr()
         *      would end up not being removed prior to test exit.
         */
        assert(info != NULL);

        assert(mptcpd_addr_info_get_id(info) == tinfo->id);
        assert(mptcpd_addr_info_get_index(info) == tinfo->ifindex);
        assert(sockaddr_is_equal(tinfo->addr,
                                 mptcpd_addr_info_get_addr(info)));
}

static void dump_addrs_callback(struct mptcpd_addr_info const *info,
                                void *user_data)
{
        struct test_info *const tinfo = (struct test_info *) user_data;

        /**
         * @bug We could have a resource leak in the kernel here if
         *      the below assert()s are triggered since addresses
         *      previously added through @c mptcpd_kpm_add_addr()
         *      would end up not being removed prior to test exit.
         */
        assert(info != NULL);

        // Other IDs unrelated to this test could already exist.
        if (mptcpd_addr_info_get_id(info) != tinfo->id)
                return;

        assert(mptcpd_addr_info_get_index(info) == tinfo->ifindex);
        assert(sockaddr_is_equal(tinfo->addr,
                                 mptcpd_addr_info_get_addr(info)));
}

static void dump_addrs_complete(void *user_data)
{
        struct test_info *const info = (struct test_info *) user_data;

        info->dump_addrs_complete_count++;
}

static void get_limits_callback(struct mptcpd_limit const *limits,
                                size_t len,
                                void *user_data)
{
        uint32_t addrs_limit = max_addrs;
        uint32_t subflows_limit = max_subflows;

        if (geteuid() != 0) {
                /*
                  If the current user is not root, the previous
                  set_limits() call fails with EPERM, but libell
                  APIs don't allow reporting such error to the caller.
                  Just assume set_limits() has no effect.
                */
                addrs_limit = 0;
                subflows_limit = 0;
        }

        (void) user_data;

        assert(limits != NULL);
        assert(len == L_ARRAY_SIZE(_limits));

        for (struct mptcpd_limit const *l = limits;
             l != limits + len;
             ++l) {
                if (l->type == MPTCPD_LIMIT_RCV_ADD_ADDRS) {
                        assert(l->limit == addrs_limit);
                } else if (l->type == MPTCPD_LIMIT_SUBFLOWS) {
                        assert(l->limit == subflows_limit);
                } else {
                        /*
                          Unless more MPTCP limit types are added to
                          the kernel path management API this should
                          never be reached.
                        */
                        l_error("Unexpected MPTCP limit type.");
                }
        }
}

// -------------------------------------------------------------------

static void test_add_addr(void const *test_data)
{
        struct test_info  *const info = (struct test_info *) test_data;
        struct mptcpd_pm  *const pm   = info->pm;
        struct mptcpd_idm *const idm  = mptcpd_pm_get_idm(pm);

        // Client-oriented path manager.
        int result = mptcpd_pm_add_addr(pm,
                                        laddr1,
                                        test_laddr_id_1,
                                        test_token_1);

        assert(result == 0 || result == ENOTSUP);

        // In-kernel (server-oriented) path manager.
        info->id = mptcpd_idm_get_id(idm, info->addr);

        uint32_t flags = 0;

        result = mptcpd_kpm_add_addr(pm,
                                     info->addr,
                                     info->id,
                                     flags,
                                     info->ifindex);

        assert(result == 0 || result == ENOTSUP);
}

static void test_remove_addr(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        // Client-oriented path manager.
        int result = mptcpd_pm_remove_addr(pm,
                                           test_laddr_id_1,
                                           test_token_1);

        assert(result == 0 || result == ENOTSUP);

        // In-kernel (server-oriented) path manager.
        result = mptcpd_kpm_remove_addr(pm, info->id);

        assert(result == 0 || result == ENOTSUP);
}

static void test_get_addr(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result =
                mptcpd_kpm_get_addr(pm,
                                    info->id,
                                    get_addr_callback,
                                    info,
                                    NULL);

        assert(result == 0 || result == ENOTSUP);
}

static void test_dump_addrs(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result =
                mptcpd_kpm_dump_addrs(pm,
                                      dump_addrs_callback,
                                      info,
                                      dump_addrs_complete);

        assert(result == 0 || result == ENOTSUP);
}

static void test_flush_addrs(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result = mptcpd_kpm_flush_addrs(pm);

        /**
         * @bug We could have a resource leak in the kernel here if
         *      the below assert()s are triggered since addresses
         *      previously added through @c mptcpd_kpm_add_addr()
         *      would end up not being removed prior to test exit.
         */
        assert(result == 0 || result == ENOTSUP);
}

static void test_set_limits(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        /**
         * @todo This call potentially overrides previously set MPTCP
         *       limits and leaves them in place after process exit.
         *       It would probably be good to restore previous
         *       values.
         */
        int const result = mptcpd_kpm_set_limits(pm,
                                                 _limits,
                                                 L_ARRAY_SIZE(_limits));

        assert(result == 0 || result == ENOTSUP);
}

static void test_get_limits(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result = mptcpd_kpm_get_limits(pm,
                                                 get_limits_callback,
                                                 NULL);

        assert(result == 0 || result == ENOTSUP);
}

static void test_set_flags(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        static mptcpd_flags_t const flags = MPTCPD_ADDR_FLAG_BACKUP;

        int const result = mptcpd_kpm_set_flags(pm, info->addr, flags);

        assert(result == 0 || result == ENOTSUP);
}

static void test_add_subflow(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result = mptcpd_pm_add_subflow(pm,
                                                 test_token_2,
                                                 test_laddr_id_2,
                                                 test_raddr_id_2,
                                                 laddr2,
                                                 raddr2,
                                                 test_backup_2);

        assert(result == 0 || result == ENOTSUP);
}

void test_set_backup(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result = mptcpd_pm_set_backup(pm,
                                                test_token_1,
                                                laddr1,
                                                raddr1,
                                                test_backup_1);

        assert(result == 0 || result == ENOTSUP);
}

void test_remove_subflow(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result = mptcpd_pm_remove_subflow(pm,
                                                    test_token_1,
                                                    laddr1,
                                                    raddr1);

        assert(result == 0 || result == ENOTSUP);
}

void test_get_nm(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        assert(mptcpd_pm_get_nm(pm) != NULL);
}

// -------------------------------------------------------------------

static void handle_rtm_newaddr(int errnum,
                               uint16_t type,
                               void const *data,
                               uint32_t len,
                               void *user_data)
{
        /*
          No data expected in underlying RTM_NEWADDR command reply
          message.
        */
        assert(type == 0);
        assert(data == NULL);
        assert(len == 0);

        if (errnum != 0) {

                static int const status = 0;  // Do not exit on error.

                struct test_info *const info = user_data;

                error(status,
                      errnum,
                      "bind of test address %s to interface %d failed",
                      info->ip,
                      info->ifindex);
        }
}

static void handle_rtm_deladdr(int errnum,
                               uint16_t type,
                               void const *data,
                               uint32_t len,
                               void *user_data)
{
        /*
          No data expected in underlying RTM_DELADDR command reply
          message.
        */
        assert(type == 0);
        assert(data == NULL);
        assert(len == 0);

        if (errnum != 0) {
                static int const status = 0;  // Do not exit on error.

                struct test_info *const info = user_data;

                error(status,
                      errnum,
                      "unbind of test address %s from "
                      "interface %d failed",
                      info->ip,
                      info->ifindex);
        }
}

// -------------------------------------------------------------------

static void run_tests(struct mptcpd_pm *pm, void *user_data)
{
        (void) pm;

        struct test_info *const t = user_data;

        l_test_run();

        t->tests_called = true;
}

static struct mptcpd_pm_ops const pm_ops = { .ready = run_tests };

static void setup_tests (void *user_data)
{
        struct test_info *const info = user_data;

        static char *argv[] = {
                "test-commands",
                "--plugin-dir",
                TEST_PLUGIN_DIR
        };
        static char **args = argv;

        static int argc = L_ARRAY_SIZE(argv);

        info->config = mptcpd_config_create(argc, argv);
        assert(info->config);

        info->pm = mptcpd_pm_create(info->config);
        assert(info->pm);

        bool const registered =
                mptcpd_pm_register_ops(info->pm, &pm_ops, info);

        assert(registered);

        l_test_init(&argc, &args);

        l_test_add("add_addr",       test_add_addr,       info);
        l_test_add("get_addr",       test_get_addr,       info);
        l_test_add("dump_addrs",     test_dump_addrs,     info);
        l_test_add("remove_addr",    test_remove_addr,    info);
        l_test_add("set_limits",     test_set_limits,     info);
        l_test_add("get_limits",     test_get_limits,     info);
        l_test_add("set_flags",      test_set_flags,      info);
        l_test_add("flush_addrs",    test_flush_addrs,    info);
        l_test_add("add_subflow",    test_add_subflow,    info);
        l_test_add("set_backup",     test_set_backup,     info);
        l_test_add("remove_subflow", test_remove_subflow, info);
        l_test_add("get_nm",         test_get_nm,         info);
}

static void complete_address_setup(void *user_data)
{
        // Run tests after address setup is complete.
        bool const result = l_idle_oneshot(setup_tests, user_data, NULL);
        assert(result);
}

static void complete_address_teardown(void *user_data)
{
        (void) user_data;

        l_main_quit();
}

static void setup_test_addresses(struct test_info *info)
{
        int const id = l_rtnl_ifaddr4_add(info->rtnl,
                                          info->ifindex,
                                          info->prefix,
                                          info->ip,
                                          NULL, // broadcast
                                          handle_rtm_newaddr,
                                          info,
                                          complete_address_setup);

        assert(id != 0);
}

static void teardown_test_addresses(struct test_info *info)
{
        int const id = l_rtnl_ifaddr4_delete(info->rtnl,
                                             info->ifindex,
                                             info->prefix,
                                             info->ip,
                                             NULL, // broadcast
                                             handle_rtm_deladdr,
                                             info,
                                             complete_address_teardown);

        assert(id != 0);
}

// -------------------------------------------------------------------

static void idle_callback(struct l_idle *idle, void *user_data)
{
        (void) idle;

        /*
          Number of ELL event loop iterations to go through before
          exiting.

          This gives the mptcpd path manager enough time to process
          replies from commands like get_addr, dump_addrs, and get_limits.
        */
        static int const trigger_count = 10;

        /*
          Maximum number of ELL event loop iterations.

          Stop the ELL event loop after this number of iterations.
         */
        static int const max_count = trigger_count * 2;

        /* ELL event loop iteration count. */
        static int count = 0;

        assert(max_count > trigger_count);  // Sanity check.

        if (++count > max_count)
                teardown_test_addresses(user_data); // Done running tests.
}

// -------------------------------------------------------------------

int main(void)
{
        // Skip this test if the kernel is not MPTCP capable.
        tests_skip_if_no_mptcp();

        if (!l_main_init())
                return -1;

        l_log_set_stderr();
        l_debug_enable("*");

        struct l_netlink *const rtnl = l_netlink_new(NETLINK_ROUTE);
        assert(rtnl != NULL);

        static struct sockaddr const *const sa = laddr1;

        // Bind test IP address to loopback interface.
        static char const loopback[] = "lo";

        char ip[INET6_ADDRSTRLEN] = { 0 };

        uint8_t prefix = 0;
        void const *src = NULL;

        if (sa->sa_family == AF_INET) {
                prefix = 32;   // One IPv4 address
                src = &((struct sockaddr_in  const *) sa)->sin_addr;
        } else {
                prefix = 128;  // One IPv6 address
                src = &((struct sockaddr_in6 const *) sa)->sin6_addr;
        }

        struct test_info info = {
                .rtnl    = rtnl,
                .addr    = sa,
                .ifindex = if_nametoindex(loopback),
                .prefix  = prefix,
                .ip      = inet_ntop(sa->sa_family,
                                     src,
                                     ip,
                                     sizeof(ip))
        };

        setup_test_addresses(&info);

        struct l_idle *const idle =
                l_idle_create(idle_callback, &info, NULL);

        (void) l_main_run();

        /*
          The tests will have run only if the MPTCP generic netlink
          family appeared.
         */
        assert(info.tests_called);

#ifdef HAVE_UPSTREAM_KERNEL
        assert(info.dump_addrs_complete_count == 1);
#endif

        l_idle_remove(idle);
        l_netlink_destroy(rtnl);
        mptcpd_pm_destroy(info.pm);
        mptcpd_config_destroy(info.config);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
