// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file path_manager_private.h
 *
 * @brief mptcpd path manager private interface.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifndef MPTCPD_PATH_MANAGER_PRIVATE_H
#define MPTCPD_PATH_MANAGER_PRIVATE_H

#include <mptcpd/types.h>


#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;

struct l_genl;
struct l_genl_family;
struct l_timeout;

struct mptcpd_pm_cmd_ops;
struct mptcpd_addr_info;
struct mptcpd_limit;
struct mptcpd_nm;
struct mptcpd_idm;

/**
 * @struct mptcpd_pm path_manager_private.h <mptcpd/private/path_manager.h>
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

        /// MPTCP path management generic netlink command functions.
        struct mptcpd_pm_cmd_ops const *cmd_ops;

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
};

/**
 * @struct mptcpd_pm_cmd_ops
 *
 * @brief MPTCP path management generic netlink command functions.
 *
 * The set of functions that implement MPTCP path management generic
 * netlink command calls.
 */
struct mptcpd_pm_cmd_ops
{
        /**
         * @privatesection
         */
        /**
         * @name Common Path Management Commands
         *
         * Path management common to both the upstream and
         * multipath-tcp.org Linux kernels.
         *
         * @param[in] pm    The mptcpd path manager object.
         * @param[in] addr  Local IP address and port to be advertised
         *                  through the MPTCP protocol @c ADD_ADDR
         *                  option.  The port is optional, and is
         *                  ignored if it is zero.
         * @param[in] id    MPTCP local address ID.
         * @param[in] flags
         * @param[in] index Network interface index (optional for
         *                  upstream Linux kernel).
         * @param[in] token MPTCP connection token.
         */
        //@{
        /**
         * @brief Advertise new network address to peers.
         */
        int (*add_addr)(struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t id,
                        uint32_t flags,
                        int index,
                        mptcpd_token_t token);

        /**
         * @brief Stop advertising network address to peers.
         */
        int (*remove_addr)(struct mptcpd_pm *pm,
                           mptcpd_aid_t address_id,
                           mptcpd_token_t token);
        //@}

        /**
         * @name Upstream Kernel Path Management Commands
         *
         * Path management commands supported by the upstream Linux
         * kernel.
         */
        //@{
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
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*get_addr)(struct mptcpd_pm *pm,
                        mptcpd_aid_t id,
                        mptcpd_pm_get_addr_cb callback,
                        void *data);

        /**
         * @brief Dump list of network addresses.
         *
         * @param[in] pm       The mptcpd path manager object.
         * @param[in] callback Function to be called when a dump of
         *                     network addresses has been retrieved.
         * @param[in] data     Data to be passed to the @a callback
         *                     function.
         *
         * @return @c 0 if operation was successful. -1 or @c errno
         *         otherwise.
         */
        int (*dump_addrs)(struct mptcpd_pm *pm,
                          mptcpd_pm_get_addr_cb callback,
                          void *data);

        /**
         * @brief Flush MPTCP addresses.
         *
         * @param[in] pm The mptcpd path manager object.
         *
         * @todo Improve documentation.
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
        //@}

        /**
         * @name multipath-tcp.org kernel Path Management Commands
         *
         * Path management commands supported by the multipath-tcp.org
         * Linux kernel.
         */
        //@{
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
        //@}
};

#ifdef __cplusplus
}
#endif

#endif /* MPTCPD_PATH_MANAGER_PRIVATE_H */

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
