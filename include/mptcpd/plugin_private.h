// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin_private.h
 *
 * @brief mptcpd private plugin interface.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#ifndef MPTCPD_PLUGIN_PRIVATE_H
#define MPTCPD_PLUGIN_PRIVATE_H

#include <stdbool.h>

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr;
struct mptcpd_pm;

/**
 * @brief Load mptcpd plugins.
 *
 * @param[in] dir          Directory from which plugins will be loaded.
 * @param[in] default_name Name of plugin to be considered the
 *                         default.
 *
 * @return @c true on successful load, @c false otherwise.
 */
MPTCPD_API bool mptcpd_plugin_load(char const *dir,
                                   char const *default_name);

/**
 * @brief Unload mptcpd plugins.
 */
MPTCPD_API void mptcpd_plugin_unload(void);

/**
 * @brief Notify plugin of new MPTCP connection pending completion.
 *
 * @param[in] name   Plugin name.
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_connection(
        char const *name,
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP connection completion.
 *
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_connection_established(
        mptcpd_token_t token,
        struct sockaddr const *laddr,
        struct sockaddr const *raddr,
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

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PLUGIN_PRIVATE_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
