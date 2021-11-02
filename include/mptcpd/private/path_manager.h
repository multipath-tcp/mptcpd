// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/path_manager.h
 *
 * @brief mptcpd path manager private interface.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_PATH_MANAGER_H
#define MPTCPD_PRIVATE_PATH_MANAGER_H

#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;

struct l_genl;
struct l_genl_family;
struct l_queue;
struct l_timeout;

struct mptcpd_netlink_pm;
struct mptcpd_addr_info;
struct mptcpd_limit;
struct mptcpd_nm;
struct mptcpd_idm;

/**
 * @struct mptcpd_pm path_manager.h <mptcpd/private/path_manager.h>
 *
 * @brief Data needed to run the path manager.
 *
 * @note The information in this structure is meant for internal use
 *       by mptcpd.  Its fields are not part of the public mptcpd
 *       API.
 */
struct mptcpd_pm
{
        /**
         * @privatesection
         */
        /// Mptcpd configuration.
        struct mptcpd_config const *config;

        /// Kernel MPTCP generic netlink path manager details.
        struct mptcpd_netlink_pm const *netlink_pm;

        /// Core ELL generic netlink object.
        struct l_genl *genl;

        /// MPTCP generic netlink multicast notification ID.
        unsigned int id;

        /**
         * @brief MPTCP generic netlink family.
         *
         * ELL generic netlink family object corresponding to the
         * MPTCP family in the kernel.
         */
        struct l_genl_family *family;

        /**
         * @brief @c "mptcp" generic netlink family timeout object.
         *
         * The timeout used to warn the user if the @c "mptcp" generic
         * netlink family needed by mptcpd does not appear within a
         * certain amount of time.
         */
        struct l_timeout *timeout;

        /**
         * @brief Network device monitor.
         *
         * The network device monitor is used to retrieve network
         * device information, such as IP addresses, as well as to
         * detect changes to network devices.
         */
        struct mptcpd_nm *nm;

        /**
         * @brief MPTCP address ID manager.
         *
         * Manager that maps IP addresses to MPTCP address IDs, and
         * generated IDs as needed..
         */
        struct mptcpd_idm *idm;

        /// List of @c pm_ops_info objects.
        struct l_queue *event_ops;
};

// -------------------------------------------------------------------

/**
 * @struct pm_ops_info
 *
 * @brief Path manager event tracking callback information.
 */
struct pm_ops_info
{
        /// Path manager event tracking operations.
        struct mptcpd_pm_ops const *ops;

        /// Data passed to the event tracking operations.
        void *user_data;
};

/**
 * @struct mptcpd_pm_cmd_ops
 *
 * @brief MPTCP path management generic netlink command functions.
 *
 * The set of functions that implement client-oriented MPTCP path
 * management generic netlink command calls where path management is
 * performed in the user space.
 */
struct mptcpd_pm_cmd_ops
{
        /**
         * @privatesection
         */
        /**
         * @brief Advertise new network address to peers.
         *
         * @param[in] pm    The mptcpd path manager object.
         * @param[in] addr  Local IP address and port to be advertised
         *                  through the MPTCP protocol @c ADD_ADDR
         *                  option.  The port is optional, and is
         *                  ignored if it is zero.
         * @param[in] id    MPTCP local address ID.
         * @param[in] token MPTCP connection token.
         */
        int (*add_addr)(struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t id,
                        mptcpd_token_t token);

        /**
         * @brief Stop advertising network address to peers.
         */
        int (*remove_addr)(struct mptcpd_pm *pm,
                           mptcpd_aid_t address_id,
                           mptcpd_token_t token);

        /**
         * @brief Create a new subflow.
         *
         * @param[in] pm                The mptcpd path manager object.
         * @param[in] token             MPTCP connection token.
         * @param[in] local_address_id  MPTCP local address ID.
         * @param[in] remote_address_id MPTCP remote address ID.
         * @param[in] local_addr        MPTCP subflow local address
         *                              information, including the port.
         * @param[in] remote_addr       MPTCP subflow remote address
         *                              information, including the port.
         * @param[in] backup            Whether or not to set the MPTCP
         *                              subflow backup priority flag.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         *
         * @todo There far too many parameters.  Reduce.
         */
        int (*add_subflow)(struct mptcpd_pm *pm,
                           mptcpd_token_t token,
                           mptcpd_aid_t local_address_id,
                           mptcpd_aid_t remote_address_id,
                           struct sockaddr const *local_addr,
                           struct sockaddr const *remote_addr,
                           bool backup);

        /**
         * @brief Remove a subflow.
         *
         * @param[in] pm                The mptcpd path manager object.
         * @param[in] token             MPTCP connection token.
         * @param[in] local_addr        MPTCP subflow local address
         *                              information, including the port.
         * @param[in] remote_addr       MPTCP subflow remote address
         *                              information, including the port.
         *
         * @return @c 0 if operation was successful. @c errno
         *         otherwise.
         */
        int (*remove_subflow)(struct mptcpd_pm *pm,
                              mptcpd_token_t token,
                              struct sockaddr const *local_addr,
                              struct sockaddr const *remote_addr);

        /**
         * @brief Set priority of a subflow.
         *
         * @param[in] pm                The mptcpd path manager object.
         * @param[in] token             MPTCP connection token.
         * @param[in] local_addr        MPTCP subflow local address
         *                              information, including the port.
         * @param[in] remote_addr       MPTCP subflow remote address
         *                              information, including the port.
         * @param[in] backup            Whether or not to set the MPTCP
         *                              subflow backup priority flag.
         *
         * @return @c 0 if operation was successful. @c errno
         *         otherwise.
         */
        int (*set_backup)(struct mptcpd_pm *pm,
                          mptcpd_token_t token,
                          struct sockaddr const *local_addr,
                          struct sockaddr const *remote_addr,
                          bool backup);
};

/**
 * @struct mptcpd_kpm_cmd_ops
 *
 * @brief Kernel-side MPTCP path management netlink commands.
 *
 * The set of functions that implement MPTCP path management generic
 * netlink command calls for the in-kernel path manager.
 */
struct mptcpd_kpm_cmd_ops
{
        /**
         * @privatesection
         */
        /**
         * @brief Advertise new network address to peers.
         *
         * @param[in] pm    The mptcpd path manager object.
         * @param[in] addr  Local IP address and port to be advertised
         *                  through the MPTCP protocol @c ADD_ADDR
         *                  option.  The port is optional, and is
         *                  ignored if it is zero.
         * @param[in] id    MPTCP local address ID.
         * @param[in] flags
         * @param[in] index Network interface index (optional).
         */
        int (*add_addr)(struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t id,
                        uint32_t flags,
                        int index);

        /**
         * @brief Stop advertising network address to peers.
         */
        int (*remove_addr)(struct mptcpd_pm *pm,
                           mptcpd_aid_t address_id);

        /**
         * @name Server-oriented Path Management Commands
         *
         * Server-oriented path management commands supported by the
         * upstream Linux kernel.  Path management is handled by the
         * kernel.
         */
        ///@{
        /**
         * @brief Get network address corresponding to an address ID.
         *
         * @param[in] pm       The mptcpd path manager object.
         * @param[in] id       MPTCP local address ID.
         * @param[in] callback Function to be called when the network
         *                     address corresponding to the given
         *                     MPTCP address @a id has been
         *                     retrieved.
         * @param[in] data     Data to be passed to the @a callback
         *                     function.
         * @param[in] complete Function called when the asynchronous
         *                     @c get_addr call completes.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*get_addr)(struct mptcpd_pm *pm,
                        mptcpd_aid_t id,
                        mptcpd_kpm_get_addr_cb_t callback,
                        void *data,
                        mptcpd_complete_func_t complete);

        /**
         * @brief Dump list of network addresses.
         *
         * @param[in] pm       The mptcpd path manager object.
         * @param[in] callback Function to be called when a dump of
         *                     network addresses has been retrieved.
         *                     This function will be called when each
         *                     address dump is available, or possibly
         *                     not at all.
         * @param[in] data     Data to be passed to the @a callback
         *                     function.
         * @param[in] complete Function called when the asynchronous
         *                     @c dump_addrs call completes.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*dump_addrs)(struct mptcpd_pm *pm,
                          mptcpd_kpm_get_addr_cb_t callback,
                          void *data,
                          mptcpd_complete_func_t complete);

        /**
         * @brief Flush MPTCP addresses.
         *
         * Purge all MPTCP addresses.
         *
         * @param[in] pm The mptcpd path manager object.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*flush_addrs)(struct mptcpd_pm *pm);

        /**
         * @brief Set MPTCP resource limits.
         *
         * @param[in] pm     The mptcpd path manager object.
         * @param[in] limits Array of MPTCP resource type/limit pairs.
         * @param[in] len    Length of the @a limits array.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*set_limits)(struct mptcpd_pm *pm,
                          struct mptcpd_limit const *limits,
                          size_t len);

        /**
         * @brief Get MPTCP resource limits.
         *
         * @param[in] pm       The mptcpd path manager object.
         * @param[in] callback Function to be called when the MPTCP
         *                     resource limits have been retrieved.
         * @param[in] data     Data to be passed to the @a callback
         *                     function.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*get_limits)(struct mptcpd_pm *pm,
                          mptcpd_pm_get_limits_cb callback,
                          void *data);

        /**
         * @brief Set MPTCP flags for a local IP address.
         *
         * @param[in] pm    The mptcpd path manager object.
         * @param[in] addr  Local IP address information.
         * @param[in] flags Flags to be associated with @a addr.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*set_flags)(struct mptcpd_pm *pm,
                         struct sockaddr const *addr,
                         mptcpd_flags_t flags);
};


#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_PRIVATE_PATH_MANAGER_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
