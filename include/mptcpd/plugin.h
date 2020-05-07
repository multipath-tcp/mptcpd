// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin.h
 *
 * @brief mptcpd user space path manager plugin header file.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifndef MPTCPD_PLUGIN_H
#define MPTCPD_PLUGIN_H

#include <stdbool.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mptcpd plugin @a "symbol" argument for @c L_PLUGIN_DEFINE.
 *
 * All mptcpd plugins are expected to use this value as the @a symbol
 * argument in their @c L_PLUGIN_DEFINE() macro call.
 */
#define MPTCPD_PLUGIN_DESC mptcpd_plugin_desc

struct sockaddr;
struct mptcpd_pm;
struct mptcpd_interface;

/**
 * @struct mptcpd_plugin_ops plugin.h <mptcpd/plugin.h>
 *
 * @brief Mptcpd plugin interface.
 *
 * This is a set of event handler callbacks that comprise the mptcpd
 * plugin API.  Mptcpd plugins should implement these event handlers
 * as needed.  Unused event handler fields may be @c NULL.
 */
struct mptcpd_plugin_ops
{
        /**
         * @name Path Manager Event Handlers
         *
         * @brief Mptcpd plugin path management event tracking
         *        operations.
         *
         * A set of functions to be called when MPTCP path management
         * related events occur.
         */
        //@{
        /**
         * @brief New MPTCP-capable connection has been created.
         *
         * A new MPTCP connection has been created, and pending
         * completion.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         */
        void (*new_connection)(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               struct mptcpd_pm *pm);

        /**
         * @brief New MPTCP-capable connection has been established.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         */
        void (*connection_established)(mptcpd_token_t token,
                                       struct sockaddr const *laddr,
                                       struct sockaddr const *raddr,
                                       struct mptcpd_pm *pm);

        /**
         * @brief MPTCP connection as a whole was closed.
         *
         * @param[in] token MPTCP connection token.
         * @param[in] pm    Opaque pointer to mptcpd path manager
         *                  object.
         */
        void (*connection_closed)(mptcpd_token_t token,
                                  struct mptcpd_pm *pm);

        /**
         * @brief New address has been advertised by a peer.
         *
         * @param[in] token MPTCP connection token.
         * @param[in] id    Remote address identifier.
         * @param[in] addr  Remote address information.
         * @param[in] pm    Opaque pointer to mptcpd path manager
         *                  object.
         *
         * Called when an address has been advertised by a peer
         * through an @c ADD_ADDR MPTCP option.
         */
        void (*new_address)(mptcpd_token_t token,
                            mptcpd_aid_t id,
                            struct sockaddr const *addr,
                            struct mptcpd_pm *pm);

        /**
         * @brief Address is no longer advertised by a peer.
         *
         * @param[in] token MPTCP connection token.
         * @param[in] id    Remote address identifier.
         * @param[in] pm    Opaque pointer to mptcpd path manager
         *                  object.
         *
         * Called when an address is no longer advertised by a peer
         * through an @c REMOVE_ADDR MPTCP option.
         */
        void (*address_removed)(mptcpd_token_t token,
                                mptcpd_aid_t id,
                                struct mptcpd_pm *pm);

        /**
         * @brief A peer has joined the MPTCP connection.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information.
         * @param[in] backup Backup priority flag.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         *
         * @note Called after a @c MP_JOIN @c ACK has been @c ACKed.
         */
        void (*new_subflow)(mptcpd_token_t token,
                            struct sockaddr const *laddr,
                            struct sockaddr const *raddr,
                            bool backup,
                            struct mptcpd_pm *pm);

        /**
         * @brief A single MPTCP subflow was closed.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information.
         * @param[in] backup Backup priority flag.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         */
        void (*subflow_closed)(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               bool backup,
                               struct mptcpd_pm *pm);

        /**
         * @brief MPTCP subflow priority changed.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information
         * @param[in] backup Backup priority flag.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         */
        void (*subflow_priority)(mptcpd_token_t token,
                                 struct sockaddr const *laddr,
                                 struct sockaddr const *raddr,
                                 bool backup,
                                 struct mptcpd_pm *pm);
        //@}

        // --------------------------------------------------------

        /**
         * @name Network Monitor Event Handlers
         *
         * @brief Mptcpd plugin network event tracking operations.
         *
         * A set of functions to be called when changes in network
         * interfaces and addresses occur.
         */
        //@{
        /**
         * @brief A new network interface is available.
         *
         * @param[in] i  Network interface information.
         * @param[in] pm Opaque pointer to mptcpd path manager
         *               object.
         *
         * @note The network address list may be empty.  Set a
         *       @c new_address callback to be notified when new
         *       network addresses become available.  Network
         *       addresses on a given network interface may be
         *       retrieved through the @c new_address callback below.
         */
        void (*new_interface)(struct mptcpd_interface const *i,
                              struct mptcpd_pm *pm);

        /**
         * @brief Network interface flags were updated.
         *
         * @param[in] i Network interface information.
         */
        void (*update_interface)(struct mptcpd_interface const *i,
                                 struct mptcpd_pm *pm);

        /**
         * @brief A network interface was removed.
         *
         * @param[in] i Network interface information.
         */
        void (*delete_interface)(struct mptcpd_interface const *i,
                                 struct mptcpd_pm *pm);

        /**
         * @brief A new local network address is available.
         *
         * @param[in] i  Network interface information.
         * @param[in] sa Network address   information.
         */
        void (*new_local_address)(struct mptcpd_interface const *i,
                                  struct sockaddr const *sa,
                                  struct mptcpd_pm *pm);

        /**
         * @brief A local network address was removed.
         *
         * @param[in] i  Network interface information.
         * @param[in] sa Network address   information.
         */
        void (*delete_local_address)(struct mptcpd_interface const *i,
                                     struct sockaddr const *sa,
                                     struct mptcpd_pm *pm);
        //@}
};

/**
 * @brief Register path manager operations.
 *
 * Path manager plugins should call this function in their @c init
 * function to register their MPTCP path manager event handling
 * functions.
 *
 * @param[in] name Plugin name.
 * @param[in] ops  Set of MPTCP path manager event handling functions
 *                 provided by the path manager plugin.
 *
 * @retval true  Registration succeeded.
 * @retval false Registration failed.  Failure should only occur if
 *               plugins were not loaded prior to calling this
 *               function.  Plugin developers should generally not
 *               have to worry about that since the load is guaranteed
 *               to have occurred prior to their @c init function
 *               being called.
 */
MPTCPD_API bool mptcpd_plugin_register_ops(
        char const *name,
        struct mptcpd_plugin_ops const *ops);

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PLUGIN_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
