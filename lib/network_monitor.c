// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file network_monitor.c
 *
 * @brief mptcpd network device monitoring.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().
#define _DEFAULT_SOURCE  ///< For standard network interface flags.

#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include <linux/rtnetlink.h>
#include <net/if.h>  // For standard network interface flags.

#include <ell/netlink.h>
#include <ell/log.h>
#include <ell/util.h>
#include <ell/queue.h>

#include <mptcpd/network_monitor.h>
#include <mptcpd/types.h>


// -------------------------------------------------------------------

/**
 * @brief Data needed to run the network monitor.
 */
struct mptcpd_nm
{
        /// ELL netlink object for the @c NETLINK_ROUTE protocol.
        struct l_netlink *rtnl;

        /// "Link" rtnetlink multicast notification IDs.
        unsigned int link_id;

        /// IPv4 rtnetlink multicast notification IDs.
        unsigned int ipv4_id;

        /// IPv6 rtnetlink multicast notification IDs.
        unsigned int ipv6_id;

        /// List of @c mptcpd_interface objects.
        struct l_queue *interfaces;
};

// -------------------------------------------------------------------
//               Network Address Information Handling
// -------------------------------------------------------------------

/**
 * @struct mptcpd_rtm_addr
 *
 * @brief Encapsulate network address information.
 *
 * This is a convenience structure that encapsulates rtnetlink
 * @c RTM_NEWADDR, @c RTM_DELADDR, and @c RTM_GETADDR message data for
 * a given network address.
 */
struct mptcpd_rtm_addr
{
        /**
         * @c RTM_NEWADDR, @c RTM_DELADDR, and @c RTM_GETADDR message
         * data.
         */
        struct ifaddrmsg const *ifa;

        /**
         * Network address retrieved from @c IFA_ADDRESS rtnetlink
         * attribute.
         */
        void const *const addr;
};

/**
 * @brief Create an object that contains internet network
 *        address-specific information.
 *
 * @param[in] info Network address-specific information retrieved from
 *                 the @c RTM_NEWADDR message.
 */
static struct mptcpd_in_addr *
mptcpd_in_addr_create(struct mptcpd_rtm_addr const *info)
{
        assert(info != NULL);

        sa_family_t const family = info->ifa->ifa_family;

        if (family != AF_INET && family != AF_INET6)
                return NULL;

        struct mptcpd_in_addr *const address =
                l_new(struct mptcpd_in_addr, 1);

        address->family = family;

        if (family == AF_INET) {
                /*
                  Kernel nla_put_in_addr() inserts a big endian 32
                  bit unsigned integer, not struct in_addr.
                */
                uint32_t const sa = *(uint32_t const*) info->addr;

                address->addr.addr4.s_addr = sa;
        } else {
                struct in6_addr const *const sa =
                        (struct in6_addr const*) info->addr;

                memcpy(&address->addr.addr6,
                       sa,
                       sizeof(*sa));
        }

        return address;
}

/**
 * @brief Destroy a @c mptcpd_in_addr object.
 *
 * @param[in,out] Pointer to the @c mptcpd_in_addr object to be
 *                destroyed.
 */
static void mptcpd_in_addr_destroy(void *data)
{
        l_free(data);
}

/**
 * @brief Compare two @c mptcpd_in_addr objects.
 *
 * Compare @c mptcpd_in_addr objects to determine where in the network
 * address list the first object, @a a, will be inserted relative to
 * the second object, @a b.
 *
 * @return Always returns 1 to make insertions append to the queue
 *         since there is no need to sort.
 *
 * @see l_queue_insert()
 */
static int
mptcpd_in_addr_compare(void const *a, void const *b, void *user_data)
{
        (void) a;
        (void) b;
        (void) user_data;

        // No need to sort.
        return 1;
}

/**
 * @brief Match an @c mptcpd_in_addr object.
 *
 * A network address represented by @a a (@c struct
 * @c mptcpd_in_addr) matches if its @c family and @c addr members
 * match those in @a b.
 *
 * @param[in] a Currently monitored network address of type @c struct
 *              @c mptcpd_in_addr*.
 * @param[in] b Network address information of type @c struct
 *              @c mptcpd_rtm_addr* to be compared against network
 *              address @a a.
 *
 * @return @c true if the network address represented by @a a matches
 *         the address @a b, and @c false otherwise.
 *
 * @see l_queue_find()
 * @see l_queue_remove_if()
 */
static bool mptcpd_in_addr_match(void const *a, void const *b)
{
        struct mptcpd_in_addr  const *const lhs = a;
        struct mptcpd_rtm_addr const *const rhs = b;

        assert(lhs);
        assert(rhs);
        assert(lhs->family == AF_INET || lhs->family == AF_INET6);

        bool matched = lhs->family == rhs->ifa->ifa_family;

        if (matched) {
                if (lhs->family == AF_INET) {
                        /*
                          Kernel nla_put_in_addr() inserts a big
                          endian 32 bit unsigned integer, not struct
                          in_addr.
                        */
                        uint32_t const sa = *(uint32_t const*) rhs->addr;

                        matched = (lhs->addr.addr4.s_addr == sa);
                } else {
                        struct in6_addr const *const sa =
                                (struct in6_addr const*) rhs->addr;

                        /**
                         * @todo Is memcmp() suitable in this case?
                         *       Do we need to worry about the
                         *       existence of uninitialized bytes in
                         *       the IPv6 address byte array.
                         */
                        matched = (memcmp(&lhs->addr.addr6.s6_addr,
                                          sa,
                                          sizeof(lhs->addr.addr6.s6_addr))
                                   == 0);
                }
        }

        return matched;
}

// -------------------------------------------------------------------
//              Network Interface Information Handling
// -------------------------------------------------------------------

/**
 * @brief Callback information supplied by the user.
 */
struct mptcpd_interface_callback_data
{
        /**
         * @brief Function to be called when iterating over network
         *        interfaces.
         */
        mptcpd_nm_callback callback;

        /**
         * @brief User supplied data.
         *
         * Data supplied by the user to be passed to the @c callback
         * function during network interface iteration.
         */
        void *user_data;
};

/**
 * @brief Create an object that contains network interface-specific
 *        information.
 *
 * @param[in] ifi Network interface-specific information retrieved
 *                from the @c RTM_NEWLINK message.
 * @param[in] len Length of the @C RTM_NEWLINK Netlink message,
 *                potentially including @c rtattr attributes.
 */
static struct mptcpd_interface *
mptcpd_interface_create(struct ifinfomsg const *ifi, uint32_t len)
{
        assert(ifi != NULL);

        struct mptcpd_interface *const interface =
                l_new(struct mptcpd_interface, 1);

        interface->family = ifi->ifi_family;
        interface->type   = ifi->ifi_type;
        interface->index  = ifi->ifi_index;
        interface->flags  = ifi->ifi_flags;

        int bytes = len - NLMSG_ALIGN(sizeof(*ifi));

        /**
         * @todo Can we retrieve the IP address associated with each
         *       network interface from IFLA_* attributes?  It seemed
         *       like we could potentially pull them from the
         *       IFLA_AF_SPEC nested attribute but that attribute
         *       appears to only contain configuration information
         *       exposed through the sysctl interface.  We may be
         *       stuck retrieving interfaces through the RTM_GETADDR
         *       rtnetlink message.
         *
         * @todo Add support for filtering networking interfaces based
         *       on whether or not they have been marked MPTCP-enabled
         *       through an mptcpd configuration/setting.
         */
        for (struct rtattr const *rta = IFLA_RTA(ifi);
             RTA_OK(rta, bytes);
             rta = RTA_NEXT(rta, bytes)) {
                switch (rta->rta_type) {
                case IFLA_IFNAME:
                        if (RTA_PAYLOAD(rta) < IF_NAMESIZE) {
                                l_strlcpy(interface->name,
                                          RTA_DATA(rta),
                                          L_ARRAY_SIZE(interface->name));

                                l_debug("link found: %s",
                                        interface->name);
                        }
                        break;
                default:
                        break;
                }
        }

        interface->addrs = l_queue_new();

        return interface;
}

/**
 * @brief Destroy a @c mptcpd_interface object.
 *
 * @param[in,out] Pointer to the @c mptcpd_interface object to be
 *                destroyed.
 */
static void mptcpd_interface_destroy(void *data)
{
        struct mptcpd_interface *const i = data;

        l_queue_destroy(i->addrs, mptcpd_in_addr_destroy);
        l_free(i);
}

/**
 * @brief Compare two @c mptcpd_interface objects.
 *
 * Compare @c mptcpd_interface objects to determine where in the
 * network interface list the first object, @a a, will be inserted
 * relative to the second object, @a b.
 *
 * @retval  1  Insert @a after  @a b.
 * @retval -1  Insert @a before @a b.
 *
 * @see l_queue_insert()
 */
static int
mptcpd_interface_compare(void const *a, void const *b, void *user_data)
{
        assert(a);
        assert(b);
        (void) user_data;

        struct mptcpd_interface const *const lhs = a;
        struct mptcpd_interface const *const rhs = b;

        // Sort by network interface index.
        return lhs->index > rhs->index ? 1 : -1;
}

/**
 * @brief Match an @c mptcpd_interface object.
 *
 * A network interface represented by @a a (@c struct
 * @c mptcpd_interface) matches if its network interface index matches
 * the provided index @a b (@c int).
 *
 * @return @c true if the index for the network interface represented
 *         by @a a matches the user supplied index @a b, and @c false
 *         otherwise.
 *
 * @see l_queue_find()
 * @see l_queue_remove_if()
 */
static bool mptcpd_interface_match(void const *a, void const *b)
{
        assert(a);
        assert(b);

        struct mptcpd_interface const *const lhs = a;
        int const *const index = b;

        return lhs->index == *index;
}

/**
 * @brief Network interface queue iteration function.
 *
 * This function will be called when iterating over the network
 * interface queue through the @c l_queue_foreach() function.
 *
 * @param[in] data      Pointer to @c mptcpd_interface object.
 * @param[in] user_data User supplied data.
 */
static void mptcpd_interface_callback(void *data, void *user_data)
{
        struct mptcpd_interface *const interface        = data;
        struct mptcpd_interface_callback_data *const cb = user_data;

        cb->callback(interface, cb->user_data);
}

// -------------------------------------------------------------------
//          Network Interface and Address Change Handling
// -------------------------------------------------------------------

/**
 * @brief Check if network interface is ready for use.
 *
 * @param[in] ifi Network interface-specific information retrieved
 *                from the @c RTM_NEWLINK message.
 *
 * @return @c true if network interface is ready, and @c false other.
 */
static bool is_interface_ready(struct ifinfomsg const *ifi)
{
        /*
          Only accept non-loopback network interfaces that are
          up and running.
        */
        static unsigned int const iff_ready = IFF_UP | IFF_RUNNING;

        return (ifi->ifi_flags & iff_ready) == iff_ready
                && (ifi->ifi_flags & IFF_LOOPBACK) == 0;
}

/**
 * @brief Register network interface (link) with network monitor.
 *
 * @param[in] ifi Network interface-specific information retrieved
 *                from the @c RTM_NEWLINK messages.
 * @param[in] len Length of the Netlink message.
 * @param[in] nm  Pointer to the @c mptcpd_nm object that contains the
 *                list (queue) to which network interface information
 *                will be inserted.
 */
static void insert_link(struct ifinfomsg const *ifi,
                        uint32_t len,
                        struct mptcpd_nm *nm)
{
        /*
          See rtnetlink(7) man page for struct ifinfomsg details.

          ifi->
            ifi_family: AF_UNSPEC
            ifi_type:   ARP protocol hardware identifer,
                        e.g. ARPHRD_ETHER (see <net/if_arp.h>)
            ifi_index:  network device/interface index
            ifi_flags:  device flags
            ifi_change: change mask
        */

        l_debug("\n"
                "ifi_family: %s\n"
                "ifi_type:   %d\n"
                "ifi_index:  %d\n"
                "ifi_flags:  0x%08x\n"
                "ifi_change: 0x%08x\n",
                ifi->ifi_family == AF_UNSPEC ? "AF_UNSPEC"
                                             : "<unexpected family>",
                ifi->ifi_type,
                ifi->ifi_index,
                ifi->ifi_flags,
                ifi->ifi_change);

        struct mptcpd_interface *const interface =
                mptcpd_interface_create(ifi, len);

        if (!l_queue_insert(nm->interfaces,
                            interface,
                            mptcpd_interface_compare,
                            NULL)) {
                mptcpd_interface_destroy(interface);

                l_error("Unable to queue network interface information.");
        }
}

/**
 * @brief Update monitored network interface (link) information.
 *
 * @param[in] ifi Network interface-specific information retrieved
 *                from the @c RTM_NEWLINK messages.
 * @param[in] len Length of the Netlink message.
 * @param[in] nm  Pointer to the @c mptcpd_nm object that contains the
 *                list (queue) to which network interface information
 *                will be inserted.
 */
static void update_link(struct ifinfomsg const *ifi,
                        uint32_t len,
                        struct mptcpd_nm *nm)
{
        struct mptcpd_interface *const i =
                l_queue_find(nm->interfaces,
                             mptcpd_interface_match,
                             &ifi->ifi_index);

        if (i == NULL)
                insert_link(ifi, len, nm);
        else
                i->flags = ifi->ifi_flags;
}

/**
 * @brief Remove network interface (link) from the network monitor.
 *
 * @param[in] ifi Network interface-specific information retrieved
 *                from @c RTM_NEWLINK (update case) or @c RTM_DELLINK
 *                messages.
 * @param[in] nm  Pointer to the @c mptcpd_nm object that contains the
 *                list (queue) to which network interface information
 *                will be inserted.
 */
static void remove_link(struct ifinfomsg const *ifi,
                        struct mptcpd_nm *nm)
{
        if (l_queue_remove_if(nm->interfaces,
                              mptcpd_interface_match,
                              &ifi->ifi_index) == NULL)
                l_debug("Network interface %d not monitored. "
                        "Ignoring monitoring removal failure.",
                        ifi->ifi_index);
}

/**
 * @brief Handle changes to network interfaces.
 *
 * This is the @c RTNLGRP_LINK message handler.
 *
 * @param[in] type      Netlink message content type.
 * @param[in] data      Pointer to rtnetlink @c ifinfomsg object
 *                      corresponding to a specific network interface.
 * @param[in] len       Length of the Netlink message.
 * @param[in] user_data Pointer to the @c mptcpd_nm object that
 *                      contains the list (queue) to which network
 *                      interface information will be inserted.
 */
static void handle_link(uint16_t type,
                        void const *data,
                        uint32_t len,
                        void *user_data)
{
        struct ifinfomsg const *const ifi = data;
        struct mptcpd_nm *const       nm  = user_data;

        switch (type) {
        case RTM_NEWLINK:
                /**
                 * @todo Inform path manager plugins of network
                 *       interface change.
                 */
                if (is_interface_ready(ifi))
                        update_link(ifi, len, nm);
                else
                        remove_link(ifi, nm);  // Interface disabled.

                break;
        case RTM_DELLINK:
                /**
                 * @todo Inform path manager plugins of network
                 *       interface change.
                 */
                remove_link(ifi, nm);

                break;
        default:
                l_error("Unexpected message in RTNLGRP_LINK handler");
                break;
        }
}

/**
 * @brief Register network address with network monitor.
 *
 * @param[in] ifa Network address-specific information retrieved
 *                from the @c RTM_NEWADDR messages.
 * @param[in] len Length of the Netlink message.
 * @param[in] i   Pointer to the @c mptcpd_interface object that
 *                contains the list (queue) to which network address
 *                information will be inserted.
 */
static void insert_addr(struct ifaddrmsg const *ifa,
                        uint32_t len,
                        struct mptcpd_interface *i)
{
        int bytes = len - NLMSG_ALIGN(sizeof(*ifa));

        for (struct rtattr const *rta = IFA_RTA(ifa);
             RTA_OK(rta, bytes);
             rta = RTA_NEXT(rta, bytes)) {
                if (rta->rta_type == IFA_ADDRESS) {
                        struct mptcpd_rtm_addr const info = {
                                .ifa  = ifa,
                                .addr = RTA_DATA(rta)
                        };

                        struct mptcpd_in_addr *const addr =
                                mptcpd_in_addr_create(&info);

                        if (unlikely(addr == NULL)
                            || !l_queue_insert(i->addrs,
                                               addr,
                                               mptcpd_in_addr_compare,
                                               NULL)) {
                                mptcpd_in_addr_destroy(addr);

                                l_error("Unable to queue internet "
                                        "address information.");
                        }
                }
        }
}

/**
 * @brief Remove network address from the network monitor.
 *
 * @param[in] ifa Network address-specific information retrieved
 *                from @c RTM_NEWADDR (update case) or @c RTM_DELADDR
 *                messages.
 * @param[in] len Length of the Netlink message.
 * @param[in] i   Pointer to the @c mptcpd_interface object that
 *                contains the list (queue) from which network address
 *                information will be removed.
 */
static void remove_addr(struct ifaddrmsg const *ifa,
                        uint32_t len,
                        struct mptcpd_interface *i)
{
        int bytes = len - NLMSG_ALIGN(sizeof(*ifa));

        for (struct rtattr const *rta = IFA_RTA(ifa);
             RTA_OK(rta, bytes);
             rta = RTA_NEXT(rta, bytes)) {
                if (rta->rta_type == IFA_ADDRESS) {
                        struct mptcpd_rtm_addr const addr = {
                                .ifa  = ifa,
                                .addr = RTA_DATA(rta)
                        };

                        if (l_queue_remove_if(i->addrs,
                                              mptcpd_in_addr_match,
                                              &addr) == NULL)
                                l_debug("Network address not monitored. "
                                        "Ignoring monitoring removal "
                                        "failure.");
                }
        }
}

/**
 * @brief Update monitored network address information.
 *
 * @param[in] ifa Network address-specific information retrieved
 *                from the @c RTM_NEWADDR messages.
 * @param[in] len Length of the Netlink message.
 * @param[in] i   Pointer to the @c mptcpd_interface object that
 *                contains the list (queue) in which network address
 *                information will be updated.
 */
static void update_addr(struct ifaddrmsg const *ifa,
                        uint32_t len,
                        struct mptcpd_interface *i)
{
        (void) ifa;
        (void) len;
        (void) i;

        /**
         * @todo Will we actually care about updating information
         *       related to an existing network address?  We only
         *       track the address itself rather than other fields in
         *       the ifaddrmsg structure (flags, etc).
         */
        l_debug("Network address information updated.");
}

/**
 * @brief Get @c mptcpd_interface instance.
 *
 * Get @c mptcpd_interface instance that matches the interface/link
 * index, i.e. @a ifa->ifa_index.
 *
 * @param[in] ifa Network address-specific information retrieved
 *                from @c RTM_NEWADDR or @c RTM_DELADDR messages.
 * @param[in] nm  Pointer to the @c mptcpd_nm object that contains the
 *                list (queue) of @c mptcpd_interface objects.
 */
static struct mptcpd_interface *get_mptcpd_interface(
        struct ifaddrmsg const * ifa,
        struct mptcpd_nm *       nm)
{
        /* See rtnetlink(7) man page for struct ifaddrmsg details. */

        /**
         * @todo The below debug log message assumes that only
         *       AF_INET and AF_INET6 families are supported by
         *       RTM_GETADDR.  While that is true at the moment, it
         *       may change in the future.
         */
        l_debug("\n"
                "ifa_family:    %s\n"
                "ifa_prefixlen: %u\n"
                "ifa_flags:     0x%02x\n"
                "ifa_scope:     %u\n"
                "ifa_index:     %d",
                ifa->ifa_family == AF_INET ? "AF_INET" : "AF_INET6",
                ifa->ifa_prefixlen,
                ifa->ifa_flags,
                ifa->ifa_scope,
                ifa->ifa_index);

        struct mptcpd_interface *const interface =
                l_queue_find(nm->interfaces,
                             mptcpd_interface_match,
                             &ifa->ifa_index);

        if (interface == NULL)
                l_debug("Ignoring address for unmonitored "
                        "network interface (%d).",
                        ifa->ifa_index);

        return interface;
}

/**
 * @brief Handle changes to network addresses.
 *
 * This is the @c RTNLGRP_IPV4_IFADDR and @c RTNLGRP_IPV6_IFADDR
 * message handler.
 *
 * @param[in] type      Netlink message content type (unused).
 * @param[in] data      Pointer to rtnetlink @c ifaddrmsg object
 *                      corresponding to a specific network interface.
 * @param[in] len       Length of the Netlink message.
 * @param[in] user_data Pointer to the @c mptcpd_nm object that
 *                      contains the list (queue) to which network
 *                      interface information will be inserted.
 */
static void handle_ifaddr(uint16_t type,
                          void const *data,
                          uint32_t len,
                          void *user_data)
{
        struct ifaddrmsg const *const ifa = data;
        struct mptcpd_nm *const       nm  = user_data;

        struct mptcpd_interface *const interface =
                get_mptcpd_interface(ifa, nm);

        /*
          Verify that the address belongs to a network interface being
          monitored.
         */
        if (interface == NULL)
                return;

        int bytes = len - NLMSG_ALIGN(sizeof(*ifa));

        for (struct rtattr const *rta = IFA_RTA(ifa);
             RTA_OK(rta, bytes);
             rta = RTA_NEXT(rta, bytes)) {
                if (rta->rta_type == IFA_ADDRESS) {
                        struct mptcpd_rtm_addr const info = {
                                .ifa  = ifa,
                                .addr = RTA_DATA(rta)
                        };

                        struct mptcpd_in_addr *const addr =
                                mptcpd_in_addr_create(&info);

                        if (unlikely(addr == NULL)
                            || !l_queue_insert(interface->addrs,
                                               addr,
                                               mptcpd_in_addr_compare,
                                               NULL))
                                l_error("Unable to track new address");
                }
        }

        switch (type) {
        case RTM_NEWADDR:
                /**
                 * @todo Inform path manager plugins of network
                 *       address change.
                 */
                update_addr(ifa, len, interface);

                break;
        case RTM_DELADDR:
                /**
                 * @todo Inform path manager plugins of network
                 *       address change.
                 */
                remove_addr(ifa, len, interface);

                break;
        default:
                l_error("Unexpected message in RTNLGRP_IPV4/V6_IFADDR "
                        "handler");
                break;
        }
}

// -------------------------------------------------------------------
//                  rtnetlink Command Handling
// -------------------------------------------------------------------

/**
 * @brief Handle error that occurred during Netlink operation.
 *
 * @param[in] fn    Name of function that failed.
 * @param[in] error Number of error (@c errno) that occurred during
 *                  Netlink operation.
 *
 * @return 0 if no error occurred, an @c errno otherwise.
 *
 * @todo There is nothing Netlink specific about this function.
 *       Should it be made available to the rest of mptcpd as a
 *       utility function?
 */
static int handle_error(char const *fn, int error)
{
        if (error != 0) {
                char errmsg[80];
                int const r =
                        strerror_r(error, errmsg, L_ARRAY_SIZE(errmsg));

                l_error("%s failed: %s",
                        fn,
                        r == 0 ? errmsg : "<unknown error>");
        }

        return error;
}

/**
 * @brief Handle results from @c RTM_GETLINK rtnetlink command.
 *
 * @param[in] error     Number of error (@c errno) that occurred
 *                      during @c RTM_GETLINK operation.
 * @param[in] type      Netlink message content type (unused).
 * @param[in] data      Pointer to rtnetlink @c ifinfomsg object
 *                      corresponding to a specific network interface.
 * @param[in] len       Length of the Netlink message (reply) for the
 *                      @c RTM_GETLINK command, potentially including
 *                      @c rtattr attributes.
 * @param[in] user_data Pointer to the @c mptcpd_nm object that
 *                      contains the list (queue) to which network
 *                      interface information will be inserted.
 */
static void handle_rtm_getlink(int error,
                               uint16_t type,
                               void const *data,
                               uint32_t len,
                               void *user_data)
{
        if (handle_error(__func__, error) != 0)
                return;

        (void) type;
        assert(type == RTM_NEWLINK);

        struct ifinfomsg const *const ifi = data;
        struct mptcpd_nm *const nm        = user_data;

        if (is_interface_ready(ifi))
                insert_link(ifi, len, nm);
}

/**
 * @brief Handle results from RTM_GETADDR rtnetlink command.
 *
 * @param[in] error     Number of error (@c errno) that occurred
 *                      during @c RTM_GETADDR operation.
 * @param[in] type      Netlink message content type (unused).
 * @param[in] data      Pointer to rtnetlink @c ifaddrmsg object
 *                      corresponding to a specific network address.
 * @param[in] len       Length of the Netlink message (reply) for the
 *                      @c RTM_GETADDR command, potentially including
 *                      @c rtattr attributes.
 * @param[in] user_data Pointer to the @c mptcpd_nm object that
 *                      contains the list (queue) to which network
 *                      address information will be inserted.
 */
static void handle_rtm_getaddr(int error,
                               uint16_t type,
                               void const *data,
                               uint32_t len,
                               void *user_data)
{
        if (handle_error(__func__, error) != 0)
                return;

        (void) type;
        assert(type == RTM_NEWADDR);

        struct ifaddrmsg const *const ifa = data;
        struct mptcpd_nm       *const nm  = user_data;

        struct mptcpd_interface *const interface =
                get_mptcpd_interface(ifa, nm);

        if (interface == NULL)
                return;

        insert_addr(ifa, len, interface);
}

/**
 * @brief Send rtnetlink command to retrieve network addresses.
 *
 * @param[in] user_data Pointer to the @c mptcpd_nm object that
 *                      contains information needed to issue the
 *                      rtnetlink RTM_GETADDR command.
 */
static void send_getaddr_command(void *user_data)
{
        struct mptcpd_nm *const nm = user_data;

        // Get IP addresses.
        struct ifaddrmsg addr_msg = { .ifa_family = AF_UNSPEC };
        if (l_netlink_send(nm->rtnl,
                           RTM_GETADDR,
                           NLM_F_DUMP,
                           &addr_msg,
                           sizeof(addr_msg),
                           handle_rtm_getaddr,
                           nm,
                           NULL) == 0) {
                l_error("Unable to obtain IP addresses.");

                /**
                 * @todo We should probably terminate mptcpd since
                 *       there isn't much we can do with out the
                 *       network addresses.
                 */
        }
}

// -------------------------------------------------------------------
//                            Public API
// -------------------------------------------------------------------

struct mptcpd_nm *mptcpd_nm_create(void)
{
        struct mptcpd_nm *const nm = l_new(struct mptcpd_nm, 1);

        // No need to check for NULL.  l_new() abort()s on failure.

        nm->rtnl = l_netlink_new(NETLINK_ROUTE);

        if (nm->rtnl == NULL) {
                l_free(nm);
                return NULL;
        }

        // Listen for link (network device) changes.
        nm->link_id = l_netlink_register(nm->rtnl,
                                         RTNLGRP_LINK,
                                         handle_link,
                                         nm,    // user_data
                                         NULL); // destroy

        if (nm->link_id == 0) {
                l_error("Unable to monitor network device changes.");
                mptcpd_nm_destroy(nm);
                return NULL;
        }

        // Listen for IPv4 address changes.
        nm->ipv4_id = l_netlink_register(nm->rtnl,
                                         RTNLGRP_IPV4_IFADDR,
                                         handle_ifaddr,
                                         nm,    // user_data
                                         NULL); // destroy

        if (nm->ipv4_id == 0) {
                l_error("Unable to monitor IPv4 address changes.");
                mptcpd_nm_destroy(nm);
                return NULL;
        }

        // Listen for IPv6 address changes.
        nm->ipv6_id = l_netlink_register(nm->rtnl,
                                         RTNLGRP_IPV6_IFADDR,
                                         handle_ifaddr,
                                         nm,    // user_data
                                         NULL); // destroy

        if (nm->ipv6_id == 0) {
                l_error("Unable to monitor IPv6 address changes.");
                mptcpd_nm_destroy(nm);
                return NULL;
        }

        nm->interfaces = l_queue_new();

        /**
         * Get network interface information.
         *
         * @note We force the second rtnetlink command, RTM_GETADDR,
         *       to be sent after this one completes by doing so in
         *       the destroy callback for this command.  That
         *       guarantees that the potential multipart message
         *       response from this dump is fully read prior to
         *       sending the second command.  This works around an
         *       issue in ELL where it was possible that the second
         *       command would be sent prior to receiving all of the
         *       parts of the multipart @c RTM_GETLINK response, which
         *       resulted in an EBUSY error.
         */
        struct ifinfomsg link_msg = { .ifi_family = AF_UNSPEC };
        if (l_netlink_send(nm->rtnl,
                           RTM_GETLINK,
                           NLM_F_DUMP,
                           &link_msg,
                           sizeof(link_msg),
                           handle_rtm_getlink,
                           nm,
                           send_getaddr_command)
            == 0) {
                l_error("Unable to obtain network devices.");
                mptcpd_nm_destroy(nm);
                return NULL;
        }

        return nm;
}

void mptcpd_nm_destroy(struct mptcpd_nm *nm)
{
        if (nm == NULL)
                return;

        if (nm->link_id != 0
            && !l_netlink_unregister(nm->rtnl, nm->link_id))
                l_error("Failed to unregister link monitor.");

        if (nm->ipv4_id != 0
            && !l_netlink_unregister(nm->rtnl, nm->ipv4_id))
                l_error("Failed to unregister IPv4 monitor.");

        if (nm->ipv6_id != 0
            && !l_netlink_unregister(nm->rtnl, nm->ipv6_id))
                l_error("Failed to unregister IPv6 monitor.");

        l_queue_destroy(nm->interfaces, mptcpd_interface_destroy);

        l_netlink_destroy(nm->rtnl);
        l_free(nm);
}

void mptcpd_nm_foreach_interface(struct mptcpd_nm const *nm,
                                 mptcpd_nm_callback callback,
                                 void *callback_data)
{
        if (nm == NULL || callback == NULL)
                return;

        struct mptcpd_interface_callback_data cb_data = {
                .callback = callback,
                .user_data = callback_data
        };

        l_queue_foreach(nm->interfaces,
                        mptcpd_interface_callback,
                        &cb_data);
}

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
