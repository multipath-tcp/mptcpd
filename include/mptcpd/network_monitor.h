// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file network_monitor.h
 *
 * @brief mptcpd network device monitoring.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifndef MPTCPD_NETWORK_MONITOR_H
#define MPTCPD_NETWORK_MONITOR_H

#include <mptcpd/export.h>

#include <net/if.h>  // For IF_NAMESIZE.

#ifdef __cplusplus
extern "C" {
#endif

struct l_queue;
struct mptcpd_nm;

/**
 * @struct mptcpd_interface network_monitor.h <mptcpd/network_monitor.h>
 *
 * @brief Network interface-specific information
 */
struct mptcpd_interface
{
        /**
         * @name Network Interface Information
         *
         * @brief Information specific to the network interface
         *        obtained through the rtnetlink API.
         *
         * @see rtnetlink(7)
         */
        ///@{
        /**
         * @brief Address family, e.g @c AF_UNSPEC.
         *
         * @note One would typically use the @c sa_family_t
         *       @c typedef, but the rtnetlink protocol itself uses
         *       @c unsigned @c char.
         */
        unsigned char family;

        /**
         * @brief Network device type, e.g. @c ARPHRD_ETHER.
         *
         * @see <net/if_arp.h>
         */
        unsigned short type;

        /// Network interface (link) index.
        int index;

        /**
         * @brief Network interface flags, e.g. @c IFF_UP.
         *
         * @see <net/if.h>
         */
        unsigned int flags;

        /// Network interface name.
        char name[IF_NAMESIZE];

        /**
         * List of IP addresses, i.e. @c sockaddr objects, associated
         * with the interface.
         */
        struct l_queue *addrs;
        ///@}
};

/**
 * @struct mptcpd_nm_ops network_monitor.h <mptcpd/network_monitor.h>
 *
 * @brief Network monitor event tracking operations.
 *
 * A set of functions to be called when changes in network interfaces
 * and addresses occur.
 */
struct mptcpd_nm_ops
{
        /**
         * @brief A new network interface is available.
         *
         * @param[in] i         Network interface information.
         * @param[in] user_data User-supplied data.
         *
         * @note The network address list may be empty.  Set a
         *       @c new_address callback to be notified when new
         *       network addresses become available.  Network
         *       addresses on a given network interface may be
         *       retrieved through the @c new_address callback below.
         */
        void (*new_interface)(struct mptcpd_interface const *i,
                              void *user_data);

        /**
         * @brief Network interface flags were updated.
         *
         * @param[in] i         Network interface information.
         * @param[in] user_data User-supplied data.
         */
        void (*update_interface)(struct mptcpd_interface const *i,
                                 void *user_data);

        /**
         * @brief A network interface was removed.
         *
         * @param[in] i         Network interface information.
         * @param[in] user_data User-supplied data.
         */
        void (*delete_interface)(struct mptcpd_interface const *i,
                                 void *user_data);

        /**
         * @brief A new network address is available.
         *
         * @param[in] i         Network interface information.
         * @param[in] sa        Network address   information.
         * @param[in] user_data User-supplied data.
         */
        void (*new_address)(struct mptcpd_interface const *i,
                            struct sockaddr const *sa,
                            void *user_data);

        /**
         * @brief A network address was removed.
         *
         * @param[in] i         Network interface information.
         * @param[in] sa        Network address   information.
         * @param[in] user_data User-supplied data.
         */
        void (*delete_address)(struct mptcpd_interface const *i,
                               struct sockaddr const *sa,
                               void *user_data);
};

/**
 * @brief Create a network monitor.
 *
 * @todo As currently implemented, one could create multiple network
 *       monitors.  Is that useful?
 *
 * @return Pointer to new network monitor on success.  @c NULL on
 *         failure.
 */
MPTCPD_API struct mptcpd_nm *mptcpd_nm_create(void);

/**
 * @brief Destroy a network monitor.
 *
 * @param[in,out] nm Network monitor to be destroyed.
 */
MPTCPD_API void mptcpd_nm_destroy(struct mptcpd_nm *nm);

/**
 * @brief Network monitor iteration function type.
 *
 * The mptcpd network monitor will call a function of this type when
 * iterating over the network interfaces via
 * @c mptcpd_nm_foreach_interface().
 *
 * @param[in]     interface     Network interface information.
 * @param[in,out] callback_data Data provided by the caller of
 *                              @c mptcpd_nm_foreach_interface().
 */
typedef void (*mptcpd_nm_callback)(
        struct mptcpd_interface const *interface,
        void *callback_data);

/**
 * @brief Iterate over all monitored network interfaces.
 *
 * @param[in] nm       Pointer to the mptcpd network monitor object.
 * @param[in] callback Function to be called during each network
 *                     interface iteration.
 * @param[in] data     Data to pass to the @a callback function during
 *                     each iteration.
 */
MPTCPD_API void mptcpd_nm_foreach_interface(struct mptcpd_nm const *nm,
                                            mptcpd_nm_callback callback,
                                            void *data);

/**
 * @brief Subscribe to mptcpd network monitor events.
 *
 * Register a set of operations that will be called on a corresponding
 * mptcpd network monitoring event, e.g. network interface or address
 * addition, update, or removal.
 *
 * @param[in,out] nm        Pointer to the mptcpd network monitor
 *                          object.
 * @param[in]     ops       Set of network monitoring event handling
 *                          functions.
 * @param[in]     user_data Data to be passed to the network event
 *                          tracking operations.
 *
 * @retval true  Registration succeeded.
 * @retval false Registration failed.
 */
MPTCPD_API bool mptcpd_nm_register_ops(struct mptcpd_nm *nm,
                                       struct mptcpd_nm_ops const *ops,
                                       void *user_data);

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_NETWORK_MONITOR_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
