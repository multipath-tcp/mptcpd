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

struct test_addr_info
{
        // Test address.
        struct sockaddr *const addr;

        // Network interface on which to bind the test address.
        int const ifindex;

        // CIDR prefix length.
        uint8_t const prefix_len;

        /**
         * @brief String form of the addr.
         *
         * @note Long enough for both IPv4 and IPv6 addresses.
         */
        char ip[INET6_ADDRSTRLEN];

        /// MPTCP connection used in user space PM calls.
        mptcpd_token_t token;

        // MPTCP address ID used for add_addr and dump_addr calls.
        mptcpd_aid_t id;
};

struct test_info
{
        struct l_netlink *const rtnl;

        /*
          Address information for user space add_addr and remove_addr
          calls.
        */
        struct test_addr_info u_addr;

        /*
          Address information for kernel add_addr and dump_addr
          calls.
        */
        struct test_addr_info k_addr;

        // Mptcpd configuration.
        struct mptcpd_config *config;

        // Mptcpd path manager object.
        struct mptcpd_pm *pm;

        // Number of times dump_addrs call was completed.
        int dump_addrs_complete_count;

        // Were the tests called?
        bool tests_called;
};

// -------------------------------------------------------------------
/*
  Number of addresses to set up for test purposes (2, user space and
  kernel space).
*/
static int const addrs_to_setup_count = 2;

// Number of addresses set up for test purposes.
static int addr_setup_count;

// -------------------------------------------------------------------
// Addresses used for user space PM subflow command tests.
// -------------------------------------------------------------------

static struct sockaddr const *const laddr2 =
        (struct sockaddr const *) &test_laddr_2;

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

static void const *get_in_addr(struct sockaddr const *sa)
{
        if (sa->sa_family == AF_INET) {
                struct sockaddr_in const *const addr =
                        (struct sockaddr_in const *) sa;

                return &addr->sin_addr;
        } else if (sa->sa_family == AF_INET6) {
                struct sockaddr_in6 const *const addr =
                        (struct sockaddr_in6 const *) sa;

                return &addr->sin6_addr;
        }

        return NULL;  // Not an internet address. Unlikely.
}

static void dump_addr(char const *description, struct sockaddr const *a)
{
        assert(a != NULL);

        void const *src = get_in_addr(a);

        char addrstr[INET6_ADDRSTRLEN];  // Long enough for both IPv4
                                         // and IPv6 addresses.

        assert(inet_ntop(a->sa_family, src, addrstr, sizeof(addrstr)));

        in_port_t const port = mptcpd_get_port_number(a);

        l_info("%s: %s:<0x%x (%u)>",
               description,
               addrstr,
               port,
               port);
}

static void get_addr_callback(struct mptcpd_addr_info const *info,
                              void *user_data)
{
        struct test_info const *const tinfo = user_data;
        struct test_addr_info const *const k_addr = &tinfo->k_addr;

        /**
         * @bug We could have a resource leak in the kernel here if
         *      the below assert()s are triggered since addresses
         *      previously added through @c mptcpd_kpm_add_addr()
         *      would end up not being removed prior to test exit.
         */
        assert(info != NULL);

        l_info("=======================");
        dump_addr("Address   to mptcpd_kpm_add_addr()",
                  k_addr->addr);
        dump_addr("Address from mptcpd_kpm_get_addr()",
                  mptcpd_addr_info_get_addr(info));
        l_info("=======================");

        assert(mptcpd_addr_info_get_id(info) == k_addr->id);
        assert(mptcpd_addr_info_get_index(info) == k_addr->ifindex);
        assert(sockaddr_is_equal(k_addr->addr,
                                 mptcpd_addr_info_get_addr(info)));
}

static void dump_addrs_callback(struct mptcpd_addr_info const *info,
                                void *user_data)
{
        struct test_info const *const tinfo = user_data;

        /**
         * @bug We could have a resource leak in the kernel here if
         *      the below assert()s are triggered since addresses
         *      previously added through @c mptcpd_kpm_add_addr()
         *      would end up not being removed prior to test exit.
         */
        assert(info != NULL);

        struct test_addr_info const *const k_addr = &tinfo->k_addr;

        // Other IDs unrelated to this test could already exist.
        if (mptcpd_addr_info_get_id(info) != k_addr->id)
                return;

        assert(mptcpd_addr_info_get_index(info) == k_addr->ifindex);
        assert(sockaddr_is_equal(k_addr->addr,
                                 mptcpd_addr_info_get_addr(info)));
}

static void dump_addrs_complete(void *user_data)
{
        struct test_info *const info = user_data;

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

static void test_get_port(void const *test_data)
{
        (void) test_data;

        struct sockaddr_in const *const addr = &test_laddr_1;

        // Network byte order port.
        in_port_t const nport = addr->sin_port;

        // Host byte order port.
        struct sockaddr const *const sa = (struct sockaddr const *) addr;
        in_port_t const hport = mptcpd_get_port_number(sa);

        /*
           Verify that mptcpd_get_port_number() returns a host byte
           order port.

           The mptcpd_get_port_number() function is used internally by
           the kernel-specific generic netlink MPTCP path management
           API implementations in mptcpd.  That API expects ports to
           be in host byte order.
        */
        assert(hport == ntohs(nport));
}

// -------------------------------------------------------------------
// In-kernel (server-oriented) path manager commands.
// -------------------------------------------------------------------
static void test_add_addr_kernel(void const *test_data)
{
        struct test_info  *const info = (struct test_info *) test_data;
        struct mptcpd_pm  *const pm   = info->pm;
        struct mptcpd_idm *const idm  = mptcpd_pm_get_idm(pm);

        struct test_addr_info *const k_addr = &info->k_addr;

        k_addr->id = mptcpd_idm_get_id(idm, k_addr->addr);

        uint32_t flags = 0;

        int const result = mptcpd_kpm_add_addr(pm,
                                               k_addr->addr,
                                               k_addr->id,
                                               flags,
                                               k_addr->ifindex);

        assert(result == 0 || result == ENOTSUP);
}

static void test_remove_addr_kernel(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        struct test_addr_info const *const k_addr = &info->k_addr;

        int const result = mptcpd_kpm_remove_addr(pm, k_addr->id);

        assert(result == 0 || result == ENOTSUP);
}

static void test_get_addr(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        int const result =
                mptcpd_kpm_get_addr(pm,
                                    info->k_addr.id,
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

        int const result =
                mptcpd_kpm_set_flags(pm, info->k_addr.addr, flags);

        assert(result == 0 || result == ENOTSUP);
}

// -------------------------------------------------------------------
// User space (client-oriented) path manager commands.
// -------------------------------------------------------------------
static void test_add_addr_user(void const *test_data)
{
        struct test_info  *const info = (struct test_info *) test_data;
        struct mptcpd_pm  *const pm   = info->pm;

        struct test_addr_info const *const u_addr = &info->u_addr;

        int const result = mptcpd_pm_add_addr(pm,
                                              u_addr->addr,
                                              u_addr->id,
                                              u_addr->token);

        /*
          EADDRNOTAVAIL error will generally occur if the test is run
          without sufficient permissions to set up the test addresses
          by associating them with a network interface.

         */
        assert(result == 0
               || result == ENOTSUP
               || result == EADDRNOTAVAIL);
}

static void test_remove_addr_user(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        struct test_addr_info const *const u_addr = &info->u_addr;

        int const result = mptcpd_pm_remove_addr(pm,
                                                 u_addr->addr,
                                                 u_addr->id,
                                                 u_addr->token);

        assert(result == 0 || result == ENOTSUP);
}

static void test_add_subflow(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        struct test_addr_info const *const u_addr = &info->u_addr;

        int const result = mptcpd_pm_add_subflow(pm,
                                                 u_addr->token,
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

        struct test_addr_info const *const u_addr = &info->u_addr;

        int const result = mptcpd_pm_set_backup(pm,
                                                u_addr->token,
                                                laddr2,
                                                raddr2,
                                                !test_backup_2);

        assert(result == 0 || result == ENOTSUP);
}

void test_remove_subflow(void const *test_data)
{
        struct test_info *const info = (struct test_info *) test_data;
        struct mptcpd_pm *const pm   = info->pm;

        struct test_addr_info const *const u_addr = &info->u_addr;

        int const result = mptcpd_pm_remove_subflow(pm,
                                                    u_addr->token,
                                                    laddr2,
                                                    raddr2);

        assert(result == 0 || result == ENOTSUP);
}

// -------------------------------------------------------------------


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

        (void) user_data;  // Pointer to struct test_info.

        if (errnum != 0) {

                static int const status = 0;  // Do not exit on error.

                error(status,
                      errnum,
                      "bind of test address to interface failed");
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

                struct test_addr_info *const info = user_data;

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

        // Non-command tests.
        l_test_add("get_port", test_get_port, NULL);
        l_test_add("get_nm",   test_get_nm,   info);

        // In-kernel path manager tests.
        l_test_add("add_addr - kernel",    test_add_addr_kernel,    info);
        l_test_add("get_addr",             test_get_addr,           info);
        l_test_add("dump_addrs",           test_dump_addrs,         info);
        l_test_add("remove_addr - kernel", test_remove_addr_kernel, info);
        l_test_add("set_limits",           test_set_limits,         info);
        l_test_add("get_limits",           test_get_limits,         info);
        l_test_add("set_flags",            test_set_flags,          info);
        l_test_add("flush_addrs",          test_flush_addrs,        info);

        // User space path manager tests.
        l_test_add("add_addr - user",    test_add_addr_user,    info);
        l_test_add("add_subflow",        test_add_subflow,      info);
        l_test_add("set_backup",         test_set_backup,       info);
        l_test_add("remove_subflow",     test_remove_subflow,   info);
        l_test_add("remove_addr - user", test_remove_addr_user, info);
}

static void complete_address_setup(void *user_data)
{
        if (++addr_setup_count == addrs_to_setup_count) {
                // Run tests after address setup is complete.
                bool const result =
                        l_idle_oneshot(setup_tests, user_data, NULL);

                assert(result);
        }
}

static void complete_address_teardown(void *user_data)
{
        (void) user_data;

        if (--addr_setup_count == 0)
                l_main_quit();
}

static void setup_test_address(struct test_info *data,
                               struct test_addr_info *info)
{
        sa_family_t const sa_family = info->addr->sa_family;

        int id = 0;

        if (sa_family == AF_INET) {
                id = l_rtnl_ifaddr4_add(data->rtnl,
                                        info->ifindex,
                                        info->prefix_len,
                                        info->ip,
                                        NULL, // broadcast
                                        handle_rtm_newaddr,
                                        data,
                                        complete_address_setup);
        } else if (sa_family == AF_INET6) {
                id = l_rtnl_ifaddr6_add(data->rtnl,
                                        info->ifindex,
                                        info->prefix_len,
                                        info->ip,
                                        handle_rtm_newaddr,
                                        data,
                                        complete_address_setup);
        }

        assert(id != 0);
}

static void setup_test_addresses(struct test_info *info)
{
        // Address used for user space PM advertising tests.
        setup_test_address(info, &info->u_addr);

        // Address used for kernel space PM tests.
        setup_test_address(info, &info->k_addr);
}

static void teardown_test_address(struct l_netlink *rtnl,
                                  struct test_addr_info *info)
{
        sa_family_t const sa_family = info->addr->sa_family;

        int id = 0;

        if (sa_family == AF_INET) {
                id = l_rtnl_ifaddr4_delete(rtnl,
                                           info->ifindex,
                                           info->prefix_len,
                                           info->ip,
                                           NULL, // broadcast
                                           handle_rtm_deladdr,
                                           info,
                                           complete_address_teardown);
        } else if (sa_family == AF_INET6) {
                id = l_rtnl_ifaddr6_delete(rtnl,
                                           info->ifindex,
                                           info->prefix_len,
                                           info->ip,
                                           handle_rtm_deladdr,
                                           info,
                                           complete_address_teardown);
        }

        assert(id != 0);
}

static void teardown_test_addresses(struct test_info *info)
{
        // Address used for user space PM advertising tests.
        teardown_test_address(info->rtnl, &info->u_addr);

        // Address used for kernel space PM tests.
        teardown_test_address(info->rtnl, &info->k_addr);
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

static uint8_t get_prefix_len(struct sockaddr const *sa)
{
        // One IP address
        return sa->sa_family == AF_INET ? 32 : 128;
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

        // Bind test IP addresses to loopback interface.
        static char const loopback[] = "lo";

        // Mutable sockaddr copies.
        struct sockaddr_in user_addr   = test_laddr_4;
        struct sockaddr_in kernel_addr = test_laddr_1;

        /*
          Set the test port to zero make the kernel choose a random
          (ephemeral) unused port to prevent potential reuse of an
          existing address.
        */
        user_addr.sin_port = 0;

        struct test_info info = {
                .rtnl    = rtnl,
                .u_addr = {
                        .addr        = (struct sockaddr *) &user_addr,
                        .ifindex     = if_nametoindex(loopback),
                        .prefix_len  = get_prefix_len(info.u_addr.addr),
                        .token       = test_token_4,
                        .id          = test_laddr_id_4
                },
                .k_addr = {
                        .addr        = (struct sockaddr *) &kernel_addr,
                        .ifindex     = if_nametoindex(loopback),
                        .prefix_len  = get_prefix_len(info.k_addr.addr)
                }
        };

        inet_ntop(info.u_addr.addr->sa_family,
                  get_in_addr(info.u_addr.addr),
                  info.u_addr.ip,
                  sizeof(info.u_addr.ip));

        inet_ntop(info.k_addr.addr->sa_family,
                  get_in_addr(info.k_addr.addr),
                  info.k_addr.ip,
                  sizeof(info.k_addr.ip));

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
