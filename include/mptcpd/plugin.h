// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin.h
 *
 * @brief mptcpd user space path manager plugin header file.
 *
 * Copyright (c) 2017-2020, 2022, Intel Corporation
 */

#ifndef MPTCPD_PLUGIN_H
#define MPTCPD_PLUGIN_H

#include <stdbool.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_pm;
struct mptcpd_interface;

/**
 * @brief Symbol name of mptcpd plugin characterstics.
 *
 * @note This is a private preprocessor constant that is not part of
 *       the mptcpd plugin API.
 */
#define MPTCPD_PLUGIN_SYM _mptcpd_plugin

/**
 * @brief Define mptcpd plugin characterstics.
 *
 * Mptcpd plugins should use this macro to define and export
 * characterstics (descriptor) required by mptcpd.
 *
 * @param[in] name        Plugin name (unquoted)
 * @param[in] description Plugin description
 * @param[in] priority    Plugin priority, where the higher values are
 *                        lower in priority, and lower values are
 *                        higher in priority, similar to how process
 *                        scheduling priorities are defined.  Mptcpd
 *                        defines convenience plugin priorities
 *                        @c MPTCP_PLUGIN_PRIORITY_LOW,
 *                        @c MPTCP_PLUGIN_PRIORITY_DEFAULT, and
 *                        @c  MPTCP_PLUGIN_PRIORITY_HIGH.
 * @param[in] init        Function called when mptcpd initializes the
 *                        plugin.
 * @param[in] exit        Function called when mptcpd finalizes the
 *                        plugin.
 */
#define MPTCPD_PLUGIN_DEFINE(name, description, priority, init, exit)   \
        extern struct mptcpd_plugin_desc const MPTCPD_PLUGIN_SYM        \
                __attribute__((visibility("default")));                 \
        struct mptcpd_plugin_desc const MPTCPD_PLUGIN_SYM = {           \
                #name,                                                  \
                description,                                            \
                0, /* version */                                        \
                priority,                                               \
                init,                                                   \
                exit                                                    \
        };

/// Low plugin priority.
#define MPTCPD_PLUGIN_PRIORITY_LOW     19

/// Default plugin priority.
#define MPTCPD_PLUGIN_PRIORITY_DEFAULT 0

/// High plugin priority.
#define MPTCPD_PLUGIN_PRIORITY_HIGH    -20

/**
 * @struct mptcpd_plugin_desc
 *
 * @brief Plugin-specific characteristics / descriptor.
 */
struct mptcpd_plugin_desc
{
        /**
         * @privatesection
         */
        /// Plugin name.
        char const *const name;

        /// Plugin description.
        char const *const description;

        /// mptcpd version against which the plugin was compiled.
        char const *const version;

        /**
         * @brief Plugin priority.
         *
         * Plugin priority, where the higher values are lower in
         * priority, and lower values are higher in priority, similar
         * to how process scheduling priorities are defined.
         *
         * @see @c MPTCP_PLUGIN_PRIORITY_LOW
         * @see @c MPTCP_PLUGIN_PRIORITY_DEFAULT
         * @see @c MPTCP_PLUGIN_PRIORITY_HIGH
         */
        int const priority;

        /// Plugin initialization function.
        int (*init)(struct mptcpd_pm *);

        /// Plugin finalization function.
        void (*exit)(struct mptcpd_pm *);
};

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
        ///@{
        /**
         * @brief New MPTCP-capable connection has been created.
         *
         * A new MPTCP connection has been created, and pending
         * completion.
         *
         * @param[in] token       MPTCP connection token.
         * @param[in] laddr       Local address information.
         * @param[in] raddr       Remote address information.
         * @param[in] server_side @c true if this peer was the
         *                        listener (server), @c false if this
         *                        peer initiated the connection.
         * @param[in] pm          Opaque pointer to mptcpd path
         *                        manager object.
         */
        void (*new_connection)(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               bool server_side,
                               struct mptcpd_pm *pm);

        /**
         * @brief New MPTCP-capable connection has been established.
         *
         * @param[in] token       MPTCP connection token.
         * @param[in] laddr       Local address information.
         * @param[in] raddr       Remote address information.
         * @param[in] server_side @c true if this peer was the
         *                        listener (server), @c false if this
         *                        peer initiated the connection.
         * @param[in] pm          Opaque pointer to mptcpd path
         *                        manager object.
         */
        void (*connection_established)(mptcpd_token_t token,
                                       struct sockaddr const *laddr,
                                       struct sockaddr const *raddr,
                                       bool server_side,
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

        /**
         * @brief New MPTCP listener socket has been created.
         *
         * @param[in] laddr       Local address information.
         * @param[in] pm          Opaque pointer to mptcpd path
         *                        manager object.
         */
        void (*listener_created)(struct sockaddr const *laddr,
                                 struct mptcpd_pm *pm);

        /**
         * @brief MPTCP listener socket has been closed.
         *
         * @param[in] laddr       Local address information.
         * @param[in] pm          Opaque pointer to mptcpd path
         *                        manager object.
         */
        void (*listener_closed)(struct sockaddr const *laddr,
                                struct mptcpd_pm *pm);
        ///@}

        // --------------------------------------------------------

        /**
         * @name Network Monitor Event Handlers
         *
         * @brief Mptcpd plugin network event tracking operations.
         *
         * A set of functions to be called when changes in network
         * interfaces and addresses occur.
         */
        ///@{
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
        ///@}
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
