// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file private/plugin.h
 *
 * @brief mptcpd private plugin interface.
 *
 * Copyright (c) 2017-2022, Intel Corporation
 */

#ifndef MPTCPD_PRIVATE_PLUGIN_H
#define MPTCPD_PRIVATE_PLUGIN_H

#include <stdbool.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>
#include <ell/queue.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_pm;
struct mptcpd_interface;

/**
 * @name MPTCP Path Manager Generic Netlink Event Handlers
 */
///@{
/**
 * @brief Load mptcpd plugins.
 *
 * @param[in] dir             Directory from which plugins will be loaded.
 * @param[in] default_name    Name of plugin to be considered the
 *                            default.
 * @param[in] plugins_to_load List of plugins to be loaded.
 * @param[in] pm              Opaque pointer to mptcpd path manager
 *                            object.
 *
 * @return @c true on successful load, @c false otherwise.
 */
MPTCPD_API bool mptcpd_plugin_load(char const *dir,
                                   char const *default_name,
                                   struct l_queue const *plugins_to_load,
                                   struct mptcpd_pm *pm);

/**
 * @brief Unload mptcpd plugins.
 *
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_unload(struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of new MPTCP connection pending completion.
 *
 * @param[in] name        Plugin name.
 * @param[in] token       MPTCP connection token.
 * @param[in] laddr       Local address information.
 * @param[in] raddr       Remote address information.
 * @param[in] server_side Server side connection flag.
 * @param[in] pm          Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_connection(
        char const *name,
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool server_side,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP connection completion.
 *
 * @param[in] token       MPTCP connection token.
 * @param[in] laddr       Local address information.
 * @param[in] raddr       Remote address information.
 * @param[in] server_side Server side connection flag.
 * @param[in] pm          Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_connection_established(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool server_side,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP connection closure.
 *
 * @param[in] token MPTCP connection token.
 * @param[in] pm    Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_connection_closed(
        mptcpd_token_t token,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of new address advertised by a peer.
 *
 * @param[in] token MPTCP connection token.
 * @param[in] id    Remote address identifier.
 * @param[in] addr  Remote address information.
 * @param[in] pm    Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_address(mptcpd_token_t token,
                                          mptcpd_aid_t id,
                                          struct sockaddr const *addr,
                                          struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of address no longer advertised by a peer.
 *
 * @param[in] token MPTCP connection token.
 * @param[in] id    Remote address identifier.
 * @param[in] pm    Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_address_removed(mptcpd_token_t token,
                                              mptcpd_aid_t id,
                                              struct mptcpd_pm *pm);

/**
 * @brief Notify plugin that a peer has joined the MPTCP connection.
 *
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] backup Backup priority flag.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_subflow(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool backup,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP subflow closure.
 *
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] backup Backup priority flag.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_subflow_closed(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool backup,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP subflow priority change.
 *
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] backup Backup priority flag.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_subflow_priority(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        bool backup,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP listener creation.
 *
 * @param[in] laddr  Local address information.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_listener_created(
        char const *name,
        struct sockaddr const *laddr,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP listener closure.
 *
 * @param[in] laddr  Local address information.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_listener_closed(
        char const *name,
        struct sockaddr const *laddr,
        struct mptcpd_pm *pm);
///@}

/**
 * @name Network Monitor Event Handlers
 *
 * A set of operations that dispatch mptcpd network monitor events to
 * all registered plugins.
 *
 * @see mptcpd_nm_ops
 */
///@{
/**
 * @brief Notify plugin of new network interface.
 *
 * @param[in] i  Network interface information.
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_interface(
        struct mptcpd_interface const *i,
        void *pm);

/**
 * @brief Notify plugin of updated network interface.
 *
 * @param[in] i  Network interface information.
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_update_interface(
        struct mptcpd_interface const *i,
        void *pm);

/**
 * @brief Notify plugin of removed network interface.
 *
 * @param[in] i  Network interface information.
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_delete_interface(
        struct mptcpd_interface const *i,
        void *pm);

/**
 * @brief Notify plugin of new network address.
 *
 * @param[in] i  Network interface information.
 * @param[in] sa Network address information.
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_local_address(
        struct mptcpd_interface const *i,
        struct sockaddr const *sa,
        void *pm);

/**
 * @brief Notify plugin of removed network address.
 *
 * @param[in] i  Network interface information.
 * @param[in] sa Network address information.
 * @param[in] pm Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_delete_local_address(
        struct mptcpd_interface const *i,
        struct sockaddr const *sa,
        void *pm);
///@}


#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PRIVATE_PLUGIN_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
