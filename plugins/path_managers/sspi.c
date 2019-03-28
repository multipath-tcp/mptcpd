// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file sspi.c
 *
 * @brief MPTCP single-subflow-per-interface path manager plugin.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#include <assert.h>
#include <stddef.h>  // For NULL.
#include <limits.h>

#include <ell/plugin.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>
#include <ell/queue.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>   // For mptcpd VERSION.
#endif

#include <mptcpd/network_monitor.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>

/**
 * @brief Local address to interface mapping failure value.
 */
#define SSPI_BAD_INDEX INT_MAX

/**
 * List of @c sspi_interface_info objects that contain MPTCP
 * connection IDs on each network interface.
 *
 * @note We could use a map, like @c l_hashmap to map network
 *       interface to the list of connection IDs, but a map seems like
 *       overkill since most platforms will have very few network
 *       interfaces.
 */
static struct l_queue *sspi_interfaces;

/**
 * @struct sspi_interface_info
 *
 * @brief Network interface information.
 *
 * This plugin tracks MPTCP subflow connection IDs on each network
 * interface.  A network interface is represented by its kernel
 * assigned index value, which is based on the local address of the
 * subflow.  Once the network interface corresponding to the subflow
 * local address is determined, the subflow connection ID is then
 * associated with the network interface as means to denote that the
 * MPTCP connection has a subflow on that network interface.
 */
struct sspi_interface_info
{
        /// Network interface index.
        int index;

        /**
         * @brief List of MPTCP connection IDs.
         *
         * A single network interface should have no duplicate
         * connection IDs, enforcing the single subflow (per
         * connection) per network interface requirement of this
         * plugin.
         */
        struct l_queue *connection_ids;
};

/**
 * @struct sspi_new_connection_info
 *
 * @brief Package @c new_connection() plugin operation arguments.
 *
 * This is a convenience structure for the purpose of making it easy
 * to pass @c new_connection() plugin operation arguments through
 * a single variable.
 */
struct sspi_new_connection_info
{
        /// Network interface index
        int index;

        /// MPTCP connection ID
        mptcpd_cid_t const connection_id;

        /// Pointer to path manager.
        struct mptcpd_pm *const pm;
};

// ----------------------------------------------------------------

/**
 * @brief Match an @c mptcpd_in_addr object.
 *
 * A network address represented by @a a (@c struct
 * @c mptcpd_in_addr) matches if its @c family and @c addr members
 * match those in the @a b.
 *
 * @return @c true if the network address represented by @a a matches
 *         the user supplied address @a b, and @c false
 *         otherwise.
 *
 * @todo This is a copy of the private @c mptcpd_in_addr_match()
 *       function.  Consider refactoring, and exposing that function
 *       through the mptcpd_nm API.
 *
 * @see l_queue_find()
 * @see l_queue_remove_if()
 */
static bool sspi_in_addr_match(void const *a, void const *b)
{
        struct mptcpd_in_addr const *const lhs = a;
        struct mptcpd_in_addr const *const rhs = b;

        assert(lhs);
        assert(rhs);
        assert(lhs->family == AF_INET || lhs->family == AF_INET6);
        assert(rhs->family == AF_INET || rhs->family == AF_INET6);

        bool matched = lhs->family == rhs->family;

        if (matched) {
                if (lhs->family == AF_INET)
                        matched = (lhs->addr.addr4.s_addr
                                   == rhs->addr.addr4.s_addr);
                else
                        /**
                         * @todo Is memcmp() suitable in this case?
                         *       Do we need to worry about the
                         *       existence of uninitialized bytes in
                         *       the IPv6 address byte array.
                         */
                        matched = (memcmp(&lhs->addr.addr6.s6_addr,
                                          &rhs->addr.addr6.s6_addr,
                                          sizeof(lhs->addr.addr6.s6_addr))
                                   == 0);
        }

        return matched;
}

/**
 * @brief Match a network interface index.
 *
 * @return @c true if the network interface index in the
 *         @c sspi_interface_info object @a a matches
 *         the user supplied index @a b, and @c false
 *         otherwise.
 *
 * @see l_queue_find()
 * @see l_queue_remove_if()
 */
static bool sspi_index_match(void const *a, void const *b)
{
        assert(a);
        assert(b);

        struct sspi_interface_info const *const info = a;
        int const *const index = b;

        return info->index == *index;
}

// ----------------------------------------------------------------

/**
 * @struct sspi_nm_callback_data
 *
 * @brief Type used to return index associated with local address.
 *
 * @see @c mptcpd_nm_callback
 */
struct sspi_nm_callback_data
{
        /// Local address information.        (IN)
        struct mptcpd_addr const* const addr;

        /// Network interface (link) index.   (OUT)
        int index;
};

/**
 * @brief Get network interface index from local address.
 *
 * @see @c mptcpd_nm_callback
 */
static void sspi_get_index(struct mptcpd_interface const *interface,
                           void *data)
{
        assert(interface->index != SSPI_BAD_INDEX);
        assert(data);

        struct sspi_nm_callback_data *const callback_data = data;

        /*
          Iterate through the network interface IP address list to
          determine which of them corresponds to the given IP address.
        */
        if (l_queue_find(interface->addrs,
                         sspi_in_addr_match,
                         &callback_data->addr->address) != NULL) {
                callback_data->index = interface->index;
        } else {
                /*
                  No network interface tracked by the mptcpd network
                  monitor with the internet address found in the
                  callback_data.
                */
                callback_data->index = SSPI_BAD_INDEX;
        }
}

/**
 * @brief Reverse lookup network interface index from IP address.
 *
 * @param[in]  nm    Mptcpd network monitor.
 * @param[in]  addr  Local address information.
 * @param[out] index Network interface (link) index.
 *
 * @return @c true if a network interface index was found that
 *         corresponds to the given local address @a addr.  @c false,
 *         otherwise.
 */
static bool sspi_addr_to_index(struct mptcpd_nm const *nm,
                               struct mptcpd_addr const *addr,
                               int *index)
{
        struct sspi_nm_callback_data data = {
                .addr = addr,
                .index = SSPI_BAD_INDEX
        };

        /**
         * @todo This iterates through the network interface list
         *       mantained by the mptcpd network monitor to find the
         *       network interface index that matches the given local
         *       address @a addr.  This could become very inefficient
         *       in the presence of a large number of network
         *       interfaces on a given platform.  Leveraging network
         *       monitor event notifications should help alleviate
         *       this issue.
         */
        mptcpd_nm_foreach_interface(nm, sspi_get_index, &data);

        *index = data.index;

        return data.index != SSPI_BAD_INDEX;
}

/**
 * @brief Compare two @c sspi_interface_info objects.
 *
 * Compare @c sspi_interface_info objects to determine where in the
 * network interface information list the first object, @a a, will be
 * inserted relative to the second object, @a b.
 *
 * @return Always returns 1 to make insertions append to the queue
 *         since there is no need to sort.
 *
 * @see l_queue_insert()
 */
static int sspi_interface_info_compare(void const *a,
                                       void const *b,
                                       void *user_data)
{
        (void) a;
        (void) b;
        (void) user_data;

        // No need to sort.
        return 1;
}

/**
 * @brief Destroy a @c sspi_interface_info object.
 *
 * @param[in,out] p Pointer to @c sspi_interface_info object.
 */
static void sspi_interface_info_destroy(void *p)
{
        if (p == NULL)
                return;

        struct sspi_interface_info *const info = p;

        l_queue_destroy(info->connection_ids, NULL);
        l_free(info);
}

/**
 * @brief Create a @c sspi_interface_info object.
 *
 * @param[in] index Network interface index.
 *
 * @return @c sspi_interface_info object with empty connection ID
 *         queue.
 */
static struct sspi_interface_info *sspi_interface_info_create(int index)
{
        struct sspi_interface_info *const info =
                l_new(struct sspi_interface_info, 1);

        info->index          = index;
        info->connection_ids = l_queue_new();

        return info;
}

/**
 * @brief Get @c sspi_interface_info object associated with @a addr.
 *
 * @param[in] nm    Mptcpd network monitor.
 * @param[in] addr  Local address information.
 *
 * @return @c sspi_interface_info object associated with @a addr, or
 *         @c NULL if retrieval failed.
 */
static struct sspi_interface_info *sspi_interface_info_lookup(
        struct mptcpd_nm const *nm,
        struct mptcpd_addr const *addr)
{
        assert(nm != NULL);
        assert(addr != NULL);

        /*
          Get the network interface index associated with the local
          address.

          This reverse lookup for the index is performed since
          multiple IP addresses may be associated with a single
          network interface.  As a result, a single address may not be
          enough to determine if a network interface is in use.
          Lookup the index from the local address instead.
        */
        int index;

        if (!sspi_addr_to_index(nm, addr, &index)) {
                l_error("No network interface with given IP address.");

                return NULL;
        }

        /*
          Check if a network interface with the provided local address
          (via the index value found above) is currently tracked by
          this plugin.
         */
        struct sspi_interface_info *info =
                l_queue_find(sspi_interfaces, sspi_index_match, &index);

        if (info == NULL) {
                /*
                  No MPTCP connections associated with the network
                  interface with the local address.  Prepare for
                  tracking of that network interface.
                */
                info = sspi_interface_info_create(index);

                if (!l_queue_insert(sspi_interfaces,
                                    info,
                                    sspi_interface_info_compare,
                                    NULL)) {
                        sspi_interface_info_destroy(info);
                        info = NULL;


                }
        }

        return NULL;
}

// ----------------------------------------------------------------

/**
 * @brief Compare two connection ID values.
 *
 * Compare connection IDs to determine where in the connection ID list
 * the first ID, @a a, will be inserted relative to the second ID,
 * @a b.
 *
 * @return Always returns 1 to make insertions append to the queue
 *         since there is no need to sort.
 *
 * @see l_queue_insert()
 */
static int sspi_cid_compare(void const *a, void const *b, void *user_data)
{
        (void) a;
        (void) b;
        (void) user_data;

        // No need to sort.
        return 1;
}

/**
 * @brief Match MPTCP connection IDs.
 *
 * @param[in] a Connection ID (via @c L_UINT_TO_PTR()).
 * @param[in] b Connection ID (via @c L_UINT_TO_PTR()).
 *
 * @return @c true if the connection IDs, @a a and @b, are equal, and
 *         @c false otherwise.
 *
 * @see l_queue_find()
 * @see l_queue_remove_if()
 */
static bool sspi_cid_match(void const *a, void const *b)
{
        mptcpd_cid_t const lhs = L_PTR_TO_UINT(a);
        mptcpd_cid_t const rhs = L_PTR_TO_UINT(b);

        return lhs == rhs;
}

/**
 * @brief Remove connection ID from tracked network interfaces.
 *
 * @param[in] data      @c sspi_interface_info object.
 * @param[in] user_data Connection ID (via @c L_UINT_TO_PTR()).
 *
 * @return @c true if @c sspi_interface_info object containing the
 *         given connection ID was removed, and @c false otherwise.
 *
 * @see l_queue_foreach_remove()
 */
static bool sspi_remove_cid(void *data, void *user_data)
{
        assert(data);
        assert(user_data);

        struct sspi_interface_info *const info = data;

        return l_queue_remove(info->connection_ids,
                              user_data);
}

// ----------------------------------------------------------------

/**
 * @brief Inform kernel of local address available for subflows.
 *
 * @param[in] i    Network interface information.
 * @param[in] data User supplied data, the path manager in this case.
 */
static void sspi_send_addr(void *data, void *user_data)
{
        struct mptcpd_in_addr           const *const ip_addr = data;
        struct sspi_new_connection_info const *const info    = user_data;

        /**
         * @bug Use real values instead of these placeholders!  The
         *      @c port, in particular, is problematic because no
         *      subflows exist for the addr in question, meaning there
         *      is no port associated with it.
         */
        mptcpd_aid_t address_id = 0;

        /**
         * @note The port is an optional field of the MPTCP
         *       @c ADD_ADDR option.  Setting it to zero causes it to
         *       be ignored sending the address information to the
         *       kernel.
         */
        struct mptcpd_addr addr = { .port = 0 };
        memcpy(&addr.address, ip_addr, sizeof(*ip_addr));

                mptcpd_pm_send_addr(info->pm,
                                    info->connection_id,
                                    address_id,
                                    &addr);
}

/**
 * @brief Inform kernel of network interface usable local addresses.
 *
 * Send all local addresses associated with the given network
 * interface if that interface doesn't already have the initial
 * subflow on it.
 *
 * @param[in] i    Network interface information.
 * @param[in] data User supplied data, the path manager in this case.
 */
static void sspi_send_addrs(struct mptcpd_interface const *i, void *data)
{
        l_debug("interface\n"
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

        struct sspi_new_connection_info *const info = data;

        /*
          Do not reuse the network interface on which the new
          connection was created.  Only one subflow per network
          interface per MPTCP connection allowed.
        */
        if (i->index != info->index) {
                /*
                  Send each address associate with the network
                  interface.
                */
                l_queue_foreach(i->addrs,
                                sspi_send_addr,
                                info);
        }
}

// ----------------------------------------------------------------
//                     Mptcpd Plugin Operations
// ----------------------------------------------------------------
static void sspi_new_connection(mptcpd_cid_t connection_id,
                                struct mptcpd_addr const *laddr,
                                struct mptcpd_addr const *raddr,
                                bool backup,
                                struct mptcpd_pm *pm)
{
        (void) raddr;
        (void) backup;

        /**
         * @note Because we directly store connection IDs in an
         *       @c l_queue by converting them to pointers via
         *       @c L_UINT_TO_PTR(), the connection ID cannot be zero
         *       since @c l_queue_find() returning a @c NULL pointer
         *       would be an ambiguous result.  Was     a match found
         *       (zero connection ID) or was it not found (@c NULL
         *       pointer)?  The connection ID cannot be zero as
         *       implemented in the kernel where it is derived from
         *       the connection token.
        */
        assert(connection_id != 0);

        struct mptcpd_nm const *const nm = mptcpd_pm_get_nm(pm);

        struct sspi_interface_info *const interface_info =
                sspi_interface_info_lookup(nm, laddr);

        if (interface_info == NULL) {
                l_error("Unable to track new connection (0x%"
                        MPTCPD_PRIxCID ")",
                        connection_id);

                return;
        }

        /*
          Associate the MPTCP connection with network interface
          corresponding to the local address.
         */
        if (!l_queue_insert(interface_info->connection_ids,
                            L_UINT_TO_PTR(connection_id),
                            sspi_cid_compare,
                            NULL)) {
                l_error("Unable to associate new connection ID "
                        "with network interface %d",
                        interface_info->index);

                return;
        }

        /*
          Inform the kernel of additional local addresses available
          for subflows, e.g. for MP_JOIN purposes.
         */
        struct sspi_new_connection_info connection_info = {
                .index         = interface_info->index,
                .connection_id = connection_id,
                .pm            = pm
        };

        mptcpd_nm_foreach_interface(nm,
                                    sspi_send_addrs,
                                    &connection_info);
}

static void sspi_new_address(mptcpd_cid_t connection_id,
                             mptcpd_aid_t addr_id,
                             struct mptcpd_addr const *addr,
                             struct mptcpd_pm *pm)
{
        (void) connection_id;
        (void) addr_id;
        (void) addr;
        (void) pm;

        /*
          The sspi plugin doesn't do anything with newly advertised
          addresses.
        */
}

static void sspi_new_subflow(mptcpd_cid_t connection_id,
                             mptcpd_aid_t laddr_id,
                             struct mptcpd_addr const *laddr,
                             mptcpd_aid_t raddr_id,
                             struct mptcpd_addr const *raddr,
                             struct mptcpd_pm *pm)
{
        (void) laddr_id;
        (void) raddr_id;

        /*
          1. Check if the new subflow local IP address corresponds to
             a network interface that already has a subflow connected
             through it, being aware that multiple IP addresses may be
             associated with a given a network interface.
          2. If the network interface corresponding to the local
             address has no subflow running on it add it to the
             connection ID to the list.  Otherwise, close the subflow.
         */

        struct mptcpd_nm const *const nm = mptcpd_pm_get_nm(pm);

        struct sspi_interface_info *const info =
                sspi_interface_info_lookup(nm, laddr);

        if (info == NULL) {
                l_error("Unable to track new subflow "
                        "(connection ID: 0x%" MPTCPD_PRIxCID ")",
                        connection_id);

                return;
        }

        if (l_queue_find(info->connection_ids,
                         sspi_cid_match,
                         L_UINT_TO_PTR(connection_id)) != NULL) {
                l_warn("Subflow already exists on network "
                       "interface (%d). "
                       "Closing new subflow.",
                        info->index);

                mptcpd_pm_remove_subflow(pm,
                                         connection_id,
                                         laddr,
                                         raddr);

                return;
        }

        /*
          Associate the MPTCP subflow with network interface
          corresponding to the local address.
         */
        if (!l_queue_insert(info->connection_ids,
                            L_UINT_TO_PTR(connection_id),
                            sspi_cid_compare,
                            NULL))
                l_error("Unable to associate new subflow connection ID "
                        "with network interface %d",
                        info->index);
}

static void sspi_subflow_closed(mptcpd_cid_t connection_id,
                                struct mptcpd_addr const *laddr,
                                struct mptcpd_addr const *raddr,
                                struct mptcpd_pm *pm)
{
        (void) raddr;

        /*
          1. Retrieve the subflow list associated with the connection
             ID.  Return immediately if no such connection ID exists,
             and log an error.
          2. Remove the subflow information associated with the given
             local IP address from the subflow list.  Return
             immediately if no subflow corresponding to the local
             address exists, and log an error.
         */

        struct mptcpd_nm const *const nm = mptcpd_pm_get_nm(pm);

        struct sspi_interface_info *const info =
                sspi_interface_info_lookup(nm, laddr);

        if (info == NULL) {
                l_error("Unable to remove subflow "
                        "(connection ID: 0x%" MPTCPD_PRIxCID ")",
                        connection_id);

                return;
        }

        if (!l_queue_remove(info->connection_ids,
                            L_UINT_TO_PTR(connection_id)))
                l_error("No subflow with connection ID "
                        "0x%" MPTCPD_PRIxCID
                        " exists on network interface %d.",
                        connection_id,
                        info->index);
}

static void sspi_connection_closed(mptcpd_cid_t connection_id,
                                   struct mptcpd_pm *pm)
{
        (void) pm;

        /*
          Remove all sspi_interface_info objects associated with the
          given connection ID.
        */
        if (l_queue_foreach_remove(sspi_interfaces,
                                   sspi_remove_cid,
                                   L_UINT_TO_PTR(connection_id)) == 0)
                l_error("No tracked connection with ID 0x%"
                        MPTCPD_PRIxCID,
                        connection_id);
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_connection    = sspi_new_connection,
        .new_address       = sspi_new_address,
        .new_subflow       = sspi_new_subflow,
        .subflow_closed    = sspi_subflow_closed,
        .connection_closed = sspi_connection_closed
};

static int sspi_init(void)
{
        // Create list of connection IDs on each network interface.
        sspi_interfaces = l_queue_new();

        static char const name[] = "sspi";

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize "
                        "single-subflow-per-interface "
                        "path manager plugin.");

                return -1;
        }

        l_info("MPTCP single-subflow-per-interface "
               "path manager initialized.");

        return 0;
}

static void sspi_exit(void)
{
        l_queue_destroy(sspi_interfaces, sspi_interface_info_destroy);

        l_info("MPTCP single-subflow-per-interface path manager exited.");
}

L_PLUGIN_DEFINE(MPTCPD_PLUGIN_DESC,
                sspi,
                "Single-subflow-per-interface path manager",
                VERSION,
                L_PLUGIN_PRIORITY_DEFAULT,
                sspi_init,
                sspi_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
