// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin.h
 *
 * @brief mptcpd user space path manager plugin header file.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#ifndef MPTCPD_PLUGIN_H
#define MPTCPD_PLUGIN_H

#include <mptcpd/export.h>
#include <mptcpd/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mptcpd plugin "@a symbol" argument for @c L_PLUGIN_DEFINE.
 *
 * All mptcpd plugins are expected to use this value as the @a symbol
 * argument in their L_PLUGIN_DEFINE() macro call.
 */
#define MPTCPD_PLUGIN_DESC mptcpd_plugin_desc

struct mptcpd_pm;

/**
 * @struct mptcpd_plugin_ops plugin.h <mptcpd/plugin.h>
 *
 * @brief Mptcpd plugin interface.
 *
 * This is a set of functions that comprise the mptcpd plugin
 * interface.  They correspond to the kernel events in the MPTCP path
 * manager generic netlink API.
 */
struct mptcpd_plugin_ops
{
        /**
         * @brief New MPTCP-capable connection has been established.
         *
         * @param[in] token  MPTCP connection token.
         * @param[in] laddr  Local address information.
         * @param[in] raddr  Remote address information.
         * @param[in] backup Backup priority flag.
         * @param[in] pm     Opaque pointer to mptcpd path manager
         *                   object.
         */
        void (*new_connection)(mptcpd_token_t token,
                               struct mptcpd_addr const *laddr,
                               struct mptcpd_addr const *raddr,
                               bool backup,
                               struct mptcpd_pm *pm);

        /**
         * @brief New address has been advertised by a peer.
         *
         * @param[in] token   MPTCP connection token.
         * @param[in] addr_id Remote address identifier.
         * @param[in] addr    Remote address information.
         * @param[in] pm      Opaque pointer to mptcpd path manager
         *                    object.
         *
         * Called when an address has been advertised by a peer
         * through an @c ADD_ADDR MPTCP option.
         *
         * @todo Why don't we have an @c address_removed() operation
         *       for the @c REMOVE_ADDR case?
         */
        void (*new_address)(mptcpd_token_t token,
                            mptcpd_aid_t addr_id,
                            struct mptcpd_addr const *addr,
                            struct mptcpd_pm *pm);

        /**
         * @brief A peer has joined the MPTCP connection.
         *
         * @param[in] token    MPTCP connection token.
         * @param[in] laddr_id Local address identifier.
         * @param[in] laddr    Local address information.
         * @param[in] raddr_id Remote address identifier.
         * @param[in] raddr    Remote address information.
         * @param[in] pm       Opaque pointer to mptcpd path manager
         *                     object.
         *
         * @note Called after a @c MP_JOIN @c ACK has been @c ACKed.
         */
        void (*new_subflow)(mptcpd_token_t token,
                            mptcpd_aid_t laddr_id,
                            struct mptcpd_addr const *laddr,
                            mptcpd_aid_t raddr_id,
                            struct mptcpd_addr const *raddr,
                            struct mptcpd_pm *pm);

        /**
         * @brief A single MPTCP subflow was closed.
         *
         * @param[in] token MPTCP connection token.
         * @param[in] laddr Local address information.
         * @param[in] raddr Remote address information
         * @param[in] pm    Opaque pointer to mptcpd path manager
         *                  object.
         */
        void (*subflow_closed)(mptcpd_token_t token,
                               struct mptcpd_addr const *laddr,
                               struct mptcpd_addr const *raddr,
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
};

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
 * @brief Register path manager operations.
 *
 * Path manager plugins should call this function to register their
 * MPTCP path manager event handling functions.
 *
 * @param[in] name Plugin name.
 * @param[in] ops  Set of MPTCP path manager event handling functions
 *                 provided by the path manager plugin.
 *
 * @retval true  Registration succeeded.
 * @retval false Registration failed.
 */
MPTCPD_API bool mptcpd_plugin_register_ops(
        char const *name,
        struct mptcpd_plugin_ops const *ops);

/**
 * @brief Notify plugin of new MPTCP connection.
 *
 * @param[in] name   Plugin name.
 * @param[in] token  MPTCP connection token.
 * @param[in] laddr  Local address information.
 * @param[in] raddr  Remote address information.
 * @param[in] backup Backup priority flag.
 * @param[in] pm     Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_connection(
        char const *name,
        mptcpd_token_t token,
        struct mptcpd_addr const *laddr,
        struct mptcpd_addr const *raddr,
        bool backup,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of new address advertised by a peer.
 *
 * @param[in] token   MPTCP connection token.
 * @param[in] addr_id Remote address identifier.
 * @param[in] addr    Remote address information.
 * @param[in] pm      Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_address(mptcpd_token_t token,
                                          mptcpd_aid_t addr_id,
                                          struct mptcpd_addr const *addr,
                                          struct mptcpd_pm *pm);

/**
 * @brief Notify plugin that a peer has joined the MPTCP connection.
 *
 * @param[in] token    MPTCP connection token.
 * @param[in] laddr_id Local address identifier.
 * @param[in] laddr    Local address information.
 * @param[in] raddr_id Remote address identifier.
 * @param[in] raddr    Remote address information.
 * @param[in] pm       Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_new_subflow(
        mptcpd_token_t token,
        mptcpd_aid_t laddr_id,
        struct mptcpd_addr const *laddr,
        mptcpd_aid_t raddr_id,
        struct mptcpd_addr const *raddr,
        struct mptcpd_pm *pm);

/**
 * @brief Notify plugin of MPTCP subflow closure.
 *
 * @param[in] token MPTCP connection token.
 * @param[in] laddr Local address information.
 * @param[in] raddr Remote address information.
 * @param[in] pm    Opaque pointer to mptcpd path manager object.
 */
MPTCPD_API void mptcpd_plugin_subflow_closed(
        mptcpd_token_t token,
        struct mptcpd_addr const *laddr,
        struct mptcpd_addr const *raddr,
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

#ifdef __cplusplus
}
#endif

#endif  // MPTCPD_PLUGIN_H


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
