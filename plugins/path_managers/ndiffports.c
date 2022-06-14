// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file ndiffports.c
 *
 * @brief MPTCP n-different-ports path manager plugin.
 *
 * This plugin creates N different subflows for each MPTCP connection,
 * all using the same single interface on each peer. Only the original
 * local and remote IP addresses are used, but with unique port numbers
 * for each subflow.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>  // For NDEBUG and mptcpd VERSION.
#endif

#include <assert.h>
#include <stddef.h>  // For NULL.
#include <limits.h>

#include <netinet/in.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>
#include <ell/hashmap.h>
#include <ell/time.h>
#pragma GCC diagnostic pop

#include <mptcpd/network_monitor.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>
#include <mptcpd/id_manager.h>

/*
 * Development notes
 *
 * - Need config file value for N in Ndiffports
 * - Track both local and remote port to allow for remote-initiated subflows
 * - Subflow request timeout (clear pending flag?)
 *
 * Questions
 * - Is subflow close "reason code" available on the genl api?
 *
 * Longer-term ideas
 *
 * - Accept advertisements for remote ports with the same remote address
 *   as the initial subflow.
 * - Advertise multiple local listening ports
 */

/**
 * @brief Number of subflows to allow.
 *
 * Compile-time constant as a temporary measure, until plugins have access
 * to config file values.
 */
#define NDIFFPORTS_LIMIT 2

/**
 * @brief Time threshold for detecting subflows rejected by the peer
 *
 * Subflows with a lifetime under this number of second are considered an
 * indication of hitting the remote peer's subflow limit.
 */
#define NDIFFPORTS_REJECT_TIME 10

/**
 * @struct ndiffports_subflow_info
 *
 * @brief Per-subflow information.
 *
 * Each connection tracks this information for each subflow it uses.
 * With ndiffports, all local and remote addresses are the same so only
 * port information is required to identify a specific subflow.
 *
 * Other fields track the state of the subflow.
 */
struct ndiffports_subflow_info
{
        /// Local port number for this subflow
        in_port_t local_port;

        /// Subflow slot is free or in use
        bool in_use;

        /// Timestamp for tracking last change to subflow state
        uint64_t timestamp;
};

/**
 * @struct ndiffports_connection_info
 *
 * @brief Per-connection information.
 *
 * Instances of this structure are stored in the ndiffports_connections
 * hashmap, indexed by token. They are created when a connection is
 * established and freed when the connection is closed or the plugin
 * exits.
 */
struct ndiffports_connection_info
{
        /// Local address for all subflows. Port number ignored.
        struct sockaddr_storage laddr;

        /// Remote address for all subflows.
        struct sockaddr_storage raddr;

        /// Server (vs client) side
        bool server_side;

        /// Active subflow count
        int active_subflows;

        /// Max subflow count
        int max_subflows;

        /// Peer rejected subflows
        int peer_rejected_consecutive;

        /// Subflow was requested, but no new_subflow event yet
        bool pending_request;

        /// Subflow info
        struct ndiffports_subflow_info subflow[NDIFFPORTS_LIMIT];
};

/**
 * Hashmap of @c ndiffports_connection_info objects that track the
 * active MPTCP connections and associated state.
 */
static struct l_hashmap *ndiffports_connections;

// ----------------------------------------------------------------
//                     Helper functions
// ----------------------------------------------------------------

/**
 * @brief Extract local port information from a @c sockaddr
 *
 * @param[in] addr A @c struct @c sockaddr with an IPv4 or IPv6 address+port
 *
 * @return @c sin_port or @c sin6_port from the @a addr, or @c 0 if
 *         an invalid @c sa_family was detected.
 *
 */
static in_port_t get_port(struct sockaddr const *addr)
{
        in_port_t port = 0;

        if (addr->sa_family == AF_INET) {
                port = ((struct sockaddr_in const *) addr)->sin_port;
        } else if (addr->sa_family == AF_INET6) {
                port = ((struct sockaddr_in6 const *) addr)->sin6_port;
        }

        return port;
}

/**
 * @brief Set the port number to 0 in a @c struct @c sockaddr.
 *
 * @param[in] addr A @c struct @c sockaddr to modify
 */
static void clear_port(struct sockaddr *addr)
{
        if (addr->sa_family == AF_INET) {
                ((struct sockaddr_in *) addr)->sin_port = 0;
        } else if (addr->sa_family == AF_INET6) {
                ((struct sockaddr_in6 *) addr)->sin6_port = 0;
        } else {
                l_error("Unexpected sockaddr family %d", addr->sa_family);
        }
}

/**
 * @brief Locate an unused subflow slot
 *
 * Each @c struct @c ndiffports_connection_info contains an array of @c
 * struct @c ndiffports_subflow_info objects. Find the first subflow
 * object that is not in use.
 *
 * @param[in] info A @c struct @c ndiffports_connection_info
 *
 * @return Pointer to the first available object, or NULL if none available
 */
static struct ndiffports_subflow_info *
find_empty_subflow(struct ndiffports_connection_info *info)
{
        for (int i = 0; i < NDIFFPORTS_LIMIT && i < info->max_subflows; i++) {
                if (!info->subflow[i].in_use) {
                        return &info->subflow[i];
                }
        }

        return NULL;
}

/**
 * @brief Find an existing subflow matching the provided local endpoint
 *
 * Look up a subflow object and return a pointer to it, using the
 * local port number. Address values are assumed to be validated already.
 *
 * @param[in] info  A @c struct @c ndiffports_connection_info
 * @param[in] laddr A @c struct @c sockaddr containing the port number
 *
 * @return Pointer to the subflow object if found, or @c NULL if not found.
 */
static struct ndiffports_subflow_info *
find_subflow(struct ndiffports_connection_info *info,
             struct sockaddr const *laddr)
{
        in_port_t lport = get_port(laddr);

        for (int i = 0; i < NDIFFPORTS_LIMIT && i < info->max_subflows; i++) {
                struct ndiffports_subflow_info *sub = &info->subflow[i];

                if (sub->in_use && sub->local_port == lport) {
                        return &info->subflow[i];
                }
        }

        return NULL;
}

/**
 * @brief Initiate a new subflow
 *
 * @param[in] pm    Path manager object to act on
 * @param[in] token Token for the connection where the subflow will be added
 * @param[in] info  Connection object where the subflow will be added
 */
static void add_new_subflow(struct mptcpd_pm *pm, mptcpd_token_t token,
                            struct ndiffports_connection_info *info)
{
        if (info->pending_request) {
                l_warn("New subflow request while previous request is pending");

                return;
        }

        info->pending_request = true;

        mptcpd_pm_add_subflow(pm, token, 1, 0,
                              (struct sockaddr *)&info->laddr,
                              (struct sockaddr *)&info->raddr,
                              false);

        /* TODO: start a timer to retry subflow creation */
}

/**
 * @brief Check two @c sockaddr objects for equality.
 *
 * Addresses are equal if the @c sa_family and @c sin_addr / @c sin6_addr
 * fields match. Port numbers are ignored.
 *
 * @param[in] addr1 Address to compare
 * @param[in] addr2 Address to compare
 *
 * @return @c true if the network addresses match (including address
 * family). @false if the address family or address values don't match.
 */
static bool addrs_equal(struct sockaddr const *addr1,
                        struct sockaddr const *addr2)
{
        if (addr1->sa_family != addr2->sa_family) {
                return false;
        }

        if (addr1->sa_family == AF_INET) {
                struct sockaddr_in const *const addr1_in =
                        (struct sockaddr_in const *) addr1;
                struct sockaddr_in const *const addr2_in =
                        (struct sockaddr_in const *) addr2;

                return addr1_in->sin_addr.s_addr == addr2_in->sin_addr.s_addr;
        } else {
                struct sockaddr_in6 const *const addr1_in6 =
                        (struct sockaddr_in6 const *) addr1;
                struct sockaddr_in6 const *const addr2_in6 =
                        (struct sockaddr_in6 const *) addr2;

                return 0 == memcmp(&addr1_in6->sin6_addr, &addr2_in6->sin6_addr,
                                   sizeof(addr1_in6->sin6_addr));
        }
}

/**
 * @brief Validate addresses on a new subflow
 *
 * With ndiffports, all subflows are expected to use the same local and
 * remote addresses as the initial subflow. Valid subflows will have
 * both addresses match (port numbers are ignored).
 *
 * @param[in] info  Connection to validate against
 * @param[in] laddr Local address for new subflow
 * @param[in] raddr Remote address for new subflow
 *
 * @return @c true if local/remote addresses for the connection match
 *         those in the new subflow. @c false if either pair differs.
 */
static bool validate_addrs(struct ndiffports_connection_info *info,
                           struct sockaddr const *laddr,
                           struct sockaddr const *raddr)
{
        return (addrs_equal((struct sockaddr*)&info->laddr, laddr) &&
                addrs_equal((struct sockaddr*)&info->raddr, raddr));
}

/**
 * @brief Calculate how many seconds ago a timestamp was
 *
 * @param[in] timestamp  Timestamp in the past
 *
 * @return Number of seconds since the @c timestamp. 0 if timestamp is in
 *         the future.
 */
static uint64_t seconds_elapsed(uint64_t timestamp)
{
        uint64_t now = l_time_now();

        return (now > timestamp) ? l_time_to_secs(now - timestamp) : 0;
}

// ----------------------------------------------------------------
//                     Mptcpd Plugin Operations
// ----------------------------------------------------------------

/**
 * @brief Connection established event handler
 *
 * When a new connection is created, set up data structures to track
 * connection-related info and track the initial subflow.
 *
 * If additional subflows need to be established and this peer initiated
 * the connection, initiate one new subflow.
 *
 * @param[in] token       MPTCP connection token for this new connection
 * @param[in] laddr       Local address and port for the initial subflow
 * @param[in] raddr       Remote address and port for the initial subflow
 * @param[in] server_side @c true if this peer was the listener (server),
 *                        @c false if this peer initiated the connection.
 * @param[in] pm          Path manager object
 */
static void ndiffports_connection_established(mptcpd_token_t token,
                                              struct sockaddr const *laddr,
                                              struct sockaddr const *raddr,
                                              bool server_side,
                                              struct mptcpd_pm *pm)
{
        struct ndiffports_subflow_info *first_subflow;
        struct ndiffports_connection_info *new_entry;
        in_port_t local_port = get_port(laddr);
        void *old_entry;

        if (!local_port) {
                l_error("Invalid local port");
                return;
        }

        new_entry = l_new(struct ndiffports_connection_info, 1);

        memcpy(&new_entry->laddr, laddr, sizeof(new_entry->laddr));
        memcpy(&new_entry->raddr, raddr, sizeof(new_entry->raddr));
        new_entry->server_side = server_side;
        new_entry->active_subflows = 1;
        new_entry->max_subflows = NDIFFPORTS_LIMIT;

        /* Zero out the port number on the local addr so it's assigned
         * at connect time for new subflows
         */
        clear_port((struct sockaddr *)&new_entry->laddr);

        first_subflow = &new_entry->subflow[0];
        first_subflow->local_port = local_port;
        first_subflow->timestamp = l_time_now();

        if (!l_hashmap_replace(ndiffports_connections, L_UINT_TO_PTR(token),
                               new_entry, &old_entry)) {
                l_error("Unable to store connection info");
        }

        if (old_entry) {
                /* Unexpected: token already exists.
                 * The kernel enforces unique tokens so assume it's stale
                 * data from an old connection that no longer exists.
                 */
                l_warn("Found old entry for token %08x", token);
                l_free(old_entry);
        }

        if (!server_side &&
            new_entry->active_subflows < new_entry->max_subflows) {
                add_new_subflow(pm, token, new_entry);
        }
}

/**
 * @brief Connection closed event handler
 *
 * When a connection is closed, delete the tracking information for
 * that connection.
 *
 * @param[in] token       MPTCP connection token for this new connection
 * @param[in] pm          Path manager object (unused)
 */
static void ndiffports_connection_closed(mptcpd_token_t token,
                                         struct mptcpd_pm *pm)
{
        struct ndiffports_connection_info *entry;
        (void) pm;

        entry = l_hashmap_remove(ndiffports_connections, L_UINT_TO_PTR(token));

        if (!entry) {
                l_warn("Missing entry for token %08x", token);
        }

        l_free(entry);
}

/**
 * @brief New subflow event handler
 *
 * When a new subflow is created, track the subflow information within
 * the corresponding connection entry.
 *
 * If additional subflows need to be established and this peer initiated
 * the overall MPTCP connection, initiate one new subflow.
 *
 * @param[in] token   MPTCP connection token for this new connection
 * @param[in] laddr   Local address and port for new subflow
 * @param[in] raddr   Remote address and port for new subflow
 * @param[in] backup  @c true if this is backup subflow, @c false for
 *                    regular priority. (not used)
 * @param[in] pm      Path manager object
 */
static void ndiffports_new_subflow(mptcpd_token_t token,
                                   struct sockaddr const *laddr,
                                   struct sockaddr const *raddr,
                                   bool backup,
                                   struct mptcpd_pm *pm)
{
        struct ndiffports_subflow_info *sub;
        struct ndiffports_connection_info *entry;

        (void) backup;

        entry = l_hashmap_lookup(ndiffports_connections, L_UINT_TO_PTR(token));

        if (!entry) {
                l_warn("New subflow for unmanaged token %08x", token);

                mptcpd_pm_remove_subflow(pm, token, laddr, raddr);
                return;
        }

        if (!validate_addrs(entry, laddr, raddr) ||
            entry->active_subflows >= entry->max_subflows) {
                goto reject;
        }

        sub = find_subflow(entry, laddr);

        if (sub) {
                l_warn("Unexpected event from established subflow");

                // TODO: stop timer.
        } else {
                sub = find_empty_subflow(entry);
                if (!sub) {
                        goto reject;
                }

                sub->in_use = true;
                sub->timestamp = l_time_now();

                entry->active_subflows++;

                sub->local_port = get_port(laddr);
        }

        sub->timestamp = l_time_now();
        entry->pending_request = false;

        /* TODO: start timer to clear consecutive_reject counter */

        if (!entry->server_side &&
            entry->active_subflows < entry->max_subflows) {
                add_new_subflow(pm, token, entry);
        }

        return;

reject:
        mptcpd_pm_remove_subflow(pm, token, laddr, raddr);
}

/**
 * @brief Subflow closed event handler
 *
 * When a subflow is closed, clear related information. If the number of
 * subflows is below the expected count and this peer initiated the
 * original connection, request a new subflow to replace the closed one.
 *
 * @param[in] token   MPTCP connection token for this new connection
 * @param[in] laddr   Local address and port for new subflow
 * @param[in] raddr   Remote address and port for new subflow
 * @param[in] backup  @c true if this is backup subflow, @c false for
 *                    regular priority. (not used)
 * @param[in] pm      Path manager object
 */
static void ndiffports_subflow_closed(mptcpd_token_t token,
                                      struct sockaddr const *laddr,
                                      struct sockaddr const *raddr,
                                      bool backup,
                                      struct mptcpd_pm *pm)
{
        struct ndiffports_connection_info *entry;
        struct ndiffports_subflow_info *sub;

        (void) backup;

        entry = l_hashmap_lookup(ndiffports_connections, L_UINT_TO_PTR(token));

        if (!entry) {
                l_warn("Closed subflow for unmanaged token %08x", token);
                return;
        }

        if (!validate_addrs(entry, laddr, raddr)) {
                l_warn("Address/token mismatch for token %08x", token);
                return;
        }

        sub = find_subflow(entry, laddr);

        if (sub) {
                if (seconds_elapsed(sub->timestamp) < NDIFFPORTS_REJECT_TIME) {
                        entry->peer_rejected_consecutive++;
                }

                sub->in_use = false;
                sub->timestamp = l_time_now();

                if (entry->active_subflows > 0) {
                        entry->active_subflows--;
                } else {
                        l_error("Underflow when adjusting subflow count");
                }
        } else {
                l_warn("Untracked subflow was closed");
        }

        if (!entry->server_side &&
            entry->active_subflows < entry->max_subflows) {
                add_new_subflow(pm, token, entry);
        }
}

/**
 * Plugin ops to register with the mptcpd core
 */
static struct mptcpd_plugin_ops const pm_ops = {
        .connection_established = ndiffports_connection_established,
        .connection_closed      = ndiffports_connection_closed,
        .new_subflow            = ndiffports_new_subflow,
        .subflow_closed         = ndiffports_subflow_closed,
};

/**
 * @brief Ndiffports plugin init hook
 *
 * Set up the hashmap for tracking connections.
 *
 * @param[in] pm  Path manager object (not used)
 */
static int ndiffports_init(struct mptcpd_pm *pm)
{
        (void) pm;

        static char const name[] = "ndiffports";

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize "
                        "n-different-ports "
                        "path manager plugin.");

                return -1;
        }

        ndiffports_connections = l_hashmap_new();

        l_info("MPTCP n-different-ports "
               "path manager initialized.");

        return 0;
}

/**
 * @brief Helper for cleaning up connection info
 *
 * Delete one connection entry.
 *
 * @param[in] pm  Path manager object (not used)
 */
static void ndiffports_connection_destroy(void *value)
{
        struct ndiffports_connection_info *info = value;

        l_free(info);
}

/**
 * @brief Ndiffports plugin exit hook
 *
 * Delete the hashmap for tracking connections.
 *
 * @param[in] pm  Path manager object (not used)
 */
static void ndiffports_exit(struct mptcpd_pm *pm)
{
        (void) pm;

        l_hashmap_destroy(ndiffports_connections,
                          ndiffports_connection_destroy);
        ndiffports_connections = NULL;
        l_info("MPTCP n-different-ports path manager exited.");
}

MPTCPD_PLUGIN_DEFINE(ndiffports,
                     "N-different-ports path manager",
                     MPTCPD_PLUGIN_PRIORITY_DEFAULT,
                     ndiffports_init,
                     ndiffports_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
