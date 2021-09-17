// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-network-monitor.c
 *
 * @brief mptcpd network monitor test.
 *
 * Copyright (c) 2018-2020, Intel Corporation
 */

#undef NDEBUG
#define _DEFAULT_SOURCE  // Enable IFF_... interface flags in <net/if.h>.

#include <assert.h>
#include <stdlib.h>

#include <arpa/inet.h>   // For inet_ntop().
#include <netinet/in.h>  // For INET_ADDRSTRLEN and INET6_ADDRSTRLEN.
#include <net/if.h>      // For standard network interface flags.

#include <ell/main.h>
#include <ell/idle.h>
#include <ell/util.h>      // Needed by <ell/log.h>
#include <ell/log.h>
#include <ell/queue.h>

#include <mptcpd/network_monitor.h>

/// Test user "data".
static int const coffee = 0xc0ffee;

/**
 * @struct foreach_data Data used when iterating over interfaces.
 *
 * @brief Test data for callback function passed to
 *        @c mptcpd_nm_foreach_interface().
 */
struct foreach_data
{
        /**
         * @brief Pointer to the mptcpd network monitor.
         */
        struct mptcpd_nm *nm;

        /**
         * Magic value that passed to the @c check_interface()
         * callback below that should match the original value passed
         * to mptcpd_nm_foreach_interface().
         */
        int cup;

        /**
         * @brief mptcpd_nm_foreach_interface() callback function call
         *        count.
         */
        int count;
};

/**
 * @brief Dump the network address for debug logging purposes.
 *
 * Print address associated with monitored network interface.
 *
 * @param[in] data      Pointer @c sockaddr object.
 * @param[in] user_data Pointer user data (unused).
 */
static void dump_addr(void *data, void *user_data)
{
        (void) user_data;

        struct sockaddr const *const a = data;
        void const *src = NULL;

        assert(a != NULL);

        if (a->sa_family == AF_INET)
                src = &((struct sockaddr_in  const *) a)->sin_addr;
        else
                src = &((struct sockaddr_in6 const *) a)->sin6_addr;

        char addrstr[INET6_ADDRSTRLEN];  // Long enough for both IPv4
                                         // and IPv6 addresses.

        assert(inet_ntop(a->sa_family, src, addrstr, sizeof(addrstr)));

        l_debug("    %s", addrstr);
}

/**
 * @brief @c mptcpd_nm_foreach_interface() test callback function.
 *
 * @param[in] if   Network interface information.
 * @param[in] data User supplied data.
 */
static void check_interface(struct mptcpd_interface const *i, void *data)
{
        assert(i != NULL);

        l_debug("\ninterface\n"
                "  family: %d\n"
                "  type:   %d\n"
                "  index:  %d\n"
                "  flags:  0x%08x\n"
                "  name:   %s",
                i->family,
                i->type,
                i->index,
                i->flags,
                i->name);

        assert(l_queue_length(i->addrs) > 0);

        l_debug("  addrs:");
        l_queue_foreach(i->addrs, dump_addr, NULL);

        /*
          Only non-loopback interfaces that are up and running should
          be monitored.
        */
        static unsigned int const ready = IFF_UP | IFF_RUNNING;
        assert(ready == (i->flags & ready));
        assert(!(i->flags & IFF_LOOPBACK));

        if (data) {
                struct foreach_data *const fdata = data;

                /*
                  Verify the user/callback data passed to and from the
                  mptcpd_nm_foreach_interface() function match.
                */
                assert(fdata->cup == coffee);

                ++fdata->count;
        }
}

// -------------------------------------------------------------------

static void idle_callback(struct l_idle *idle, void *user_data)
{
        (void) idle;

        /*
          Number of ELL event loop iterations to go through before
          triggering the mptcpd_nm_foreach_interface() call.

          This gives the mptcpd network monitor enough time to
          populate its network interface list since the list is
          (necessarily) populated over several ELL event loop
          iterations.
        */
        static int const trigger_count = 10;

        /*
          Maximum number of ELL event loop iterations.

          Stop the ELL event loop after this number of iterations.
         */
        static int const max_count = trigger_count * 2;

        /* ELL event loop iteration count. */
        static int count = 0;

        struct foreach_data *const data = user_data;

        assert(max_count > trigger_count);  // Sanity check.

        /*
          The mptcpd network monitor interface list should now be
          populated.  Iterate through the list, and verify that the
          monitored interfaces are what we expect.
         */
        if (count > trigger_count && data->count == 0)
                mptcpd_nm_foreach_interface(data->nm,
                                            check_interface,
                                            data);

        if (++count > max_count)
                l_main_quit();
}

static void handle_new_interface(struct mptcpd_interface const *i,
                                 void *user_data)
{
        l_debug("new_interface event occurred.");

        check_interface(i, NULL);

        assert((int const *) user_data == &coffee);
}

static void handle_update_interface(struct mptcpd_interface const *i,
                                    void *user_data)
{
        l_debug("update_interface event occurred.");

        check_interface(i, NULL);

        assert((int const *) user_data == &coffee);
}

static void handle_delete_interface(struct mptcpd_interface const *i,
                                    void *user_data)
{
        l_debug("delete_interface event occurred.");

        check_interface(i, NULL);

        assert((int const *) user_data == &coffee);
}

void handle_new_address(struct mptcpd_interface const *i,
                        struct sockaddr const *sa,
                        void *user_data)
{
        l_debug("new_address event occurred.");

        check_interface(i, NULL);

        l_debug("  new address:");
        dump_addr((void *) sa, NULL);

        assert((int const *) user_data == &coffee);
}

void handle_delete_address(struct mptcpd_interface const *i,
                           struct sockaddr const *sa,
                           void *user_data)
{
        l_debug("delete_address event occurred.");

        check_interface(i, NULL);

        l_debug("  deleted address:");
        dump_addr((void *) sa, NULL);

        assert((int const *) user_data == &coffee);
}

int main(void)
{
        if (!l_main_init())
                return -1;

        l_log_set_stderr();
        l_debug_enable("*");

        struct mptcpd_nm *const nm = mptcpd_nm_create(0);
        assert(nm);

        struct mptcpd_nm_ops const nm_events[] = {
                {
                        .new_interface    = handle_new_interface,
                        .update_interface = handle_update_interface,
                        .delete_interface = handle_delete_interface,
                        .new_address      = handle_new_address,
                        .delete_address   = handle_delete_address
                },
                {
                        .new_interface    = handle_new_interface,
                        .update_interface = handle_update_interface,
                        .delete_interface = handle_delete_interface
                },
                {
                        .new_address      = handle_new_address,
                        .delete_address   = handle_delete_address
                }
        };

        // Subscribe to network monitoring related events.
        for (size_t i = 0; i < L_ARRAY_SIZE(nm_events); ++i)
                mptcpd_nm_register_ops(nm,
                                       &nm_events[i],
                                       (void *) &coffee);

        struct foreach_data data = { .nm = nm, .cup = coffee };

        // Prepare to iterate over all monitored network interfaces.
        struct l_idle *const idle =
                l_idle_create(idle_callback, &data, NULL);

        (void) l_main_run();

        mptcpd_nm_destroy(nm);

        l_idle_remove(idle);

        // Make sure the foreach test callback was actually called.
        assert(data.count > 0);

        return l_main_exit() ? 0 : -1;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
