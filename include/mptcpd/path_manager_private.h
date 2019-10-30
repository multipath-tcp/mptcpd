// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file path_manager_private.h
 *
 * @brief mptcpd user space path manager plugin header file.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#ifndef MPTCPD_PATH_MANAGER_PRIVATE_H
#define MPTCPD_PATH_MANAGER_PRIVATE_H

#ifdef __cplusplus
extern "C" {
#endif

struct l_genl;
struct l_genl_family;
struct l_timeout;

struct mptcpd_nm;

/**
 * @struct mptcpd_pm path_manager_private.h <mptcpd/path_manager_private.h>
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
