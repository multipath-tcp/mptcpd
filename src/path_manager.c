// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file src/path_manager.c
 *
 * @brief mptcpd path manager framework.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <unistd.h>

#include <arpa/inet.h>   // For inet_ntop().
#include <netinet/in.h>

#include <ell/genl.h>
#include <ell/log.h>
#include <ell/queue.h>
#include <ell/timeout.h>
#include <ell/util.h>

#include <mptcpd/path_manager.h>
#include <mptcpd/private/path_manager.h>
#include <mptcpd/private/plugin.h>
#include <mptcpd/network_monitor.h>
#include <mptcpd/private/id_manager.h>
#include <mptcpd/id_manager.h>
#include <mptcpd/private/sockaddr.h>
#include <mptcpd/private/configuration.h>
#include <mptcpd/addr_info.h>

// For netlink events.  Same API applies to multipath-tcp.org kernel.
#include <mptcpd/private/mptcp_upstream.h>

#include "path_manager.h"
#include "netlink_pm.h"


static unsigned int const FAMILY_TIMEOUT_SECONDS = 10;

/**
 * @brief Validate generic netlink attribute size.
 *
 * @param[in] actual   Actual   attribute size.
 * @param[in] expected Expected attribute size.
 *
 * @retval true  Attribute size is valid.
 * @retval false Attribute size is invalid.
 */
static bool validate_attr_len(size_t actual, size_t expected)
{
        bool const is_valid = (actual == expected);

        if (!is_valid)
                l_error("Attribute length (%zu) is "
                        "not the expected length (%zu)",
                        actual,
                        expected);

        return is_valid;
}

/**
 * @brief Retrieve generic netlink attribute.
 *
 * This macro is basically a function with a built-in sanity
 * check that casts @c void* typed data to a variable of desired
 * type.
 *
 * @param[in]  data Pointer to source attribute data.
 * @param[in]  len  Length (size) of attribute data.
 * @param[out] attr Pointer to attribute data destination.
 */
#define MPTCP_GET_NL_ATTR(data, len, attr)                      \
        do {                                                    \
                if (validate_attr_len(len, sizeof(*(attr))))    \
                        (attr) = data;                          \
        } while(0)

#ifdef MPTCPD_ENABLE_PM_NAME
static char const *get_pm_name(void const *data, size_t len)
{
        char const *pm_name = NULL;

        if (data != NULL) {
                if (validate_attr_len(len, (size_t) MPTCP_PM_NAME_LEN))
                        pm_name = data;
                else
                        l_error("Path manager name length (%zu) "
                                "is not the expected length "
                                "(%d)",
                                len,
                                MPTCP_PM_NAME_LEN);
        }

        return pm_name;
}
#endif // MPTCPD_ENABLE_PM_NAME

static void handle_connection_created(struct l_genl_msg *msg,
                                      void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return;
        }

        /*
          Payload:
              Token
              Local address
              Local port
              Remote address
              Remote port
              Path management strategy (optional)
        */

        mptcpd_token_t  const *token       = NULL;
        struct in_addr  const *laddr4      = NULL;
        struct in_addr  const *raddr4      = NULL;
        struct in6_addr const *laddr6      = NULL;
        struct in6_addr const *raddr6      = NULL;
        in_port_t       const *local_port  = NULL;
        in_port_t       const *remote_port = NULL;
        char            const *pm_name     = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, token);
                        break;
                case MPTCP_ATTR_SADDR4:
                        MPTCP_GET_NL_ATTR(data, len, laddr4);
                        break;
                case MPTCP_ATTR_SADDR6:
                        MPTCP_GET_NL_ATTR(data, len, laddr6);
                        break;
                case MPTCP_ATTR_SPORT:
                        MPTCP_GET_NL_ATTR(data, len, local_port);
                        break;
                case MPTCP_ATTR_DADDR4:
                        MPTCP_GET_NL_ATTR(data, len, raddr4);
                        break;
                case MPTCP_ATTR_DADDR6:
                        MPTCP_GET_NL_ATTR(data, len, raddr6);
                        break;
                case MPTCP_ATTR_DPORT:
                        MPTCP_GET_NL_ATTR(data, len, remote_port);
                        break;
                case MPTCP_ATTR_FAMILY:
                case MPTCP_ATTR_LOC_ID:
                case MPTCP_ATTR_REM_ID:
                case MPTCP_ATTR_BACKUP:
                case MPTCP_ATTR_IF_IDX:
                        // Unused and ignored, at least for now.
                        break;
#ifdef MPTCPD_ENABLE_PM_NAME
                case MPTCP_ATTR_PATH_MANAGER:
                        pm_name = get_pm_name(data, len);
                        break;
#endif  // MPTCPD_ENABLE_PM_NAME
                default:
                        l_warn("Unknown MPTCP_EVENT_CREATED "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!token
            || !(laddr4 || laddr6)
            || !local_port
            || !(raddr4 || raddr6)
            || !remote_port) {
                l_error("Required MPTCP_EVENT_CREATED "
                        "message attributes are missing.");

                return;
        }

        struct sockaddr_storage laddr, raddr;

        if (!mptcpd_sockaddr_storage_init(laddr4,
                                          laddr6,
                                          *local_port,
                                          &laddr)
            || !mptcpd_sockaddr_storage_init(raddr4,
                                             raddr6,
                                             *remote_port,
                                             &raddr)) {
                l_error("Unable to initialize address information");

                return;
        }

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_new_connection(pm_name,
                                     *token,
                                     (struct sockaddr *) &laddr,
                                     (struct sockaddr *) &raddr,
                                     pm);
}

static void handle_connection_established(struct l_genl_msg *msg,
                                          void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return;
        }

        /*
          Payload:
              Token
              Local address
              Local port
              Remote address
              Remote port
        */

        mptcpd_token_t  const *token       = NULL;
        struct in_addr  const *laddr4      = NULL;
        struct in_addr  const *raddr4      = NULL;
        struct in6_addr const *laddr6      = NULL;
        struct in6_addr const *raddr6      = NULL;
        in_port_t       const *local_port  = NULL;
        in_port_t       const *remote_port = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, token);
                        break;
                case MPTCP_ATTR_SADDR4:
                        MPTCP_GET_NL_ATTR(data, len, laddr4);
                        break;
                case MPTCP_ATTR_SADDR6:
                        MPTCP_GET_NL_ATTR(data, len, laddr6);
                        break;
                case MPTCP_ATTR_SPORT:
                        MPTCP_GET_NL_ATTR(data, len, local_port);
                        break;
                case MPTCP_ATTR_DADDR4:
                        MPTCP_GET_NL_ATTR(data, len, raddr4);
                        break;
                case MPTCP_ATTR_DADDR6:
                        MPTCP_GET_NL_ATTR(data, len, raddr6);
                        break;
                case MPTCP_ATTR_DPORT:
                        MPTCP_GET_NL_ATTR(data, len, remote_port);
                        break;
                case MPTCP_ATTR_FAMILY:
                case MPTCP_ATTR_LOC_ID:
                case MPTCP_ATTR_REM_ID:
                case MPTCP_ATTR_BACKUP:
                case MPTCP_ATTR_IF_IDX:
                        // Unused and ignored, at least for now.
                        break;
                default:
                        l_warn("Unknown MPTCP_EVENT_ESTABLISHED "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!token
            || !(laddr4 || laddr6)
            || !local_port
            || !(raddr4 || raddr6)
            || !remote_port) {
                l_error("Required MPTCP_EVENT_ESTABLISHED "
                        "message attributes are missing.");

                return;
        }

        struct sockaddr_storage laddr, raddr;

        if (!mptcpd_sockaddr_storage_init(laddr4,
                                          laddr6,
                                          *local_port,
                                          &laddr)
            || !mptcpd_sockaddr_storage_init(raddr4,
                                             raddr6,
                                             *remote_port,
                                             &raddr)) {
                l_error("Unable to initialize address information");

                return;
        }

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_connection_established(*token,
                                             (struct sockaddr *) &laddr,
                                             (struct sockaddr *) &raddr,
                                             pm);
}

static void handle_connection_closed(struct l_genl_msg *msg,
                                     void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return;
        }

        /*
          Payload:
              Token
         */

        mptcpd_token_t const *token = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, token);
                        break;
                default:
                        l_warn("Unknown MPTCP_EVENT_CLOSED "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!token) {
                l_error("Required MPTCP_EVENT_CLOSED "
                        "message attributes are missing.");

                return;
        }

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_connection_closed(*token, pm);
}

static void handle_new_addr(struct l_genl_msg *msg, void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return;
        }

        /*
          Payload:
              Token
              Remote address ID
              Remote address
              Remote port (optional)
        */

        mptcpd_token_t  const *token      = NULL;
        mptcpd_aid_t    const *address_id = NULL;
        struct in_addr  const *addr4      = NULL;
        struct in6_addr const *addr6      = NULL;
        in_port_t       const *port       = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, token);
                        break;
                case MPTCP_ATTR_REM_ID:
                        MPTCP_GET_NL_ATTR(data, len, address_id);
                        break;
                case MPTCP_ATTR_DADDR4:
                        MPTCP_GET_NL_ATTR(data, len, addr4);
                        break;
                case MPTCP_ATTR_DADDR6:
                        MPTCP_GET_NL_ATTR(data, len, addr6);
                        break;
                case MPTCP_ATTR_DPORT:
                        MPTCP_GET_NL_ATTR(data, len, port);
                        break;
                default:
                        l_warn("Unknown MPTCP_EVENT_ANNOUNCED "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!token || !address_id || !(addr4 || addr6)) {
                l_error("Required MPTCP_EVENT_ANNOUNCED "
                        "message attributes are missing.");

                return;
        }

        struct sockaddr_storage addr;

        if (!mptcpd_sockaddr_storage_init(addr4,
                                          addr6,
                                          port ? *port : 0,
                                          &addr)) {
                l_error("Unable to initialize address information");

                return;
        }

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_new_address(*token,
                                  *address_id,
                                  (struct sockaddr *) &addr,
                                  pm);
}

static void handle_addr_removed(struct l_genl_msg *msg, void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return;
        }

        /*
          Payload:
              Token
              Remote address ID
              Remote address
              Remote port (optional)
        */

        mptcpd_token_t  const *token      = NULL;
        mptcpd_aid_t    const *address_id = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, token);
                        break;
                case MPTCP_ATTR_REM_ID:
                        MPTCP_GET_NL_ATTR(data, len, address_id);
                        break;
                default:
                        l_warn("Unknown MPTCP_EVENT_REMOVED "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!token || !address_id) {
                l_error("Required MPTCP_EVENT_REMOVED "
                        "message attributes are missing.");

                return;
        }

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_address_removed(*token, *address_id, pm);
}

/**
 * @brief Retrieve subflow event attributes.
 *
 * All subflow events have the same payload attributes.  Share
 * attribute retrieval in one location.
 *
 * @param[in]  msg    Generic netlink MPTCP subflow event message.
 * @param[out] token  MPTCP connection token.
 * @param[out] laddr  MPTCP subflow local  address and port.
 * @param[out] raddr  MPTCP subflow remote address and port.
 * @param[out] backup MPTCP subflow backup priority flag.
 *
 * @return @c true on success, @c false otherwise.
 */
static bool handle_subflow(struct l_genl_msg *msg,
                           mptcpd_token_t *token,
                           struct sockaddr_storage *laddr,
                           struct sockaddr_storage *raddr,
                           bool *backup)
{
        assert(token != NULL);
        assert(laddr != NULL);
        assert(raddr != NULL);
        assert(backup != NULL);

        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
                return false;
        }

        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
              Backup priority
              Network interface index
              Error (optional)
         */

        mptcpd_token_t  const *tok         = NULL;
        struct in_addr  const *laddr4      = NULL;
        struct in_addr  const *raddr4      = NULL;
        struct in6_addr const *laddr6      = NULL;
        struct in6_addr const *raddr6      = NULL;
        in_port_t       const *local_port  = NULL;
        in_port_t       const *remote_port = NULL;
        uint8_t         const *bkup        = NULL;

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                switch (type) {
                case MPTCP_ATTR_TOKEN:
                        MPTCP_GET_NL_ATTR(data, len, tok);
                        break;
                case MPTCP_ATTR_SADDR4:
                        MPTCP_GET_NL_ATTR(data, len, laddr4);
                        break;
                case MPTCP_ATTR_SADDR6:
                        MPTCP_GET_NL_ATTR(data, len, laddr6);
                        break;
                case MPTCP_ATTR_SPORT:
                        MPTCP_GET_NL_ATTR(data, len, local_port);
                        break;
                case MPTCP_ATTR_DADDR4:
                        MPTCP_GET_NL_ATTR(data, len, raddr4);
                        break;
                case MPTCP_ATTR_DADDR6:
                        MPTCP_GET_NL_ATTR(data, len, raddr6);
                        break;
                case MPTCP_ATTR_DPORT:
                        MPTCP_GET_NL_ATTR(data, len, remote_port);
                        break;
                case MPTCP_ATTR_BACKUP:
                        MPTCP_GET_NL_ATTR(data, len, bkup);
                        break;
                default:
                        l_warn("Unknown MPTCP_EVENT_SUB_* "
                               "attribute: %d",
                               type);
                        break;
                }
        }

        if (!tok
            || !(laddr4 || laddr6)
            || !local_port
            || !(raddr4 || raddr6)
            || !remote_port
            || !bkup) {
                l_error("Required MPTCP_EVENT_SUB_* "
                        "message attributes are missing.");

                return false;
        }

        *token = *tok;

        if (!mptcpd_sockaddr_storage_init(laddr4,
                                          laddr6,
                                          *local_port,
                                          laddr)
            || !mptcpd_sockaddr_storage_init(raddr4,
                                             raddr6,
                                             *remote_port,
                                             raddr)) {
                l_error("Unable to initialize address information");

                return false;
        }

        *backup = *bkup;

        return true;
}

static void handle_new_subflow(struct l_genl_msg *msg, void *user_data)
{
        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
              Backup priority
              Network interface index
              Error (optional)
         */

        mptcpd_token_t token = 0;
        struct sockaddr_storage laddr;
        struct sockaddr_storage raddr;
        bool backup = false;

        if (!handle_subflow(msg, &token, &laddr, &raddr, &backup))
                return;

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_new_subflow(token,
                                  (struct sockaddr *) &laddr,
                                  (struct sockaddr *) &raddr,
                                  backup,
                                  pm);
}

static void handle_subflow_closed(struct l_genl_msg *msg, void *user_data)
{
        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
              Backup priority
              Network interface index
              Error (optional)
         */

        mptcpd_token_t token = 0;
        struct sockaddr_storage laddr;
        struct sockaddr_storage raddr;
        bool backup = false;

        if (!handle_subflow(msg, &token, &laddr, &raddr, &backup))
                return;

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_subflow_closed(token,
                                     (struct sockaddr *) &laddr,
                                     (struct sockaddr *) &raddr,
                                     backup,
                                     pm);
}

static void handle_priority_changed(struct l_genl_msg *msg,
                                    void *user_data)
{
        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
              Backup
              Network interface index
              Error (optional)
         */

        mptcpd_token_t token = 0;
        struct sockaddr_storage laddr;
        struct sockaddr_storage raddr;
        bool backup = false;

        if (!handle_subflow(msg, &token, &laddr, &raddr, &backup))
                return;

        struct mptcpd_pm *const pm = user_data;

        mptcpd_plugin_subflow_priority(token,
                                       (struct sockaddr *) &laddr,
                                       (struct sockaddr *) &raddr,
                                       backup,
                                       pm);
}

static void handle_mptcp_event(struct l_genl_msg *msg, void *user_data)
{
        int const cmd = l_genl_msg_get_command(msg);

        assert(cmd != 0);

        switch (cmd) {
        case MPTCP_EVENT_CREATED:
                handle_connection_created(msg, user_data);
                break;

        case MPTCP_EVENT_ESTABLISHED:
                handle_connection_established(msg, user_data);
                break;

        case MPTCP_EVENT_CLOSED:
                handle_connection_closed(msg, user_data);
                break;

        case MPTCP_EVENT_ANNOUNCED:
                handle_new_addr(msg, user_data);
                break;

        case MPTCP_EVENT_REMOVED:
                handle_addr_removed(msg, user_data);
                break;

        case MPTCP_EVENT_SUB_ESTABLISHED:
                handle_new_subflow(msg, user_data);
                break;

        case MPTCP_EVENT_SUB_CLOSED:
                handle_subflow_closed(msg, user_data);
                break;

        case MPTCP_EVENT_SUB_PRIORITY:
                handle_priority_changed(msg, user_data);
                break;

        default:
                l_error("Unhandled MPTCP event: %d", cmd);
                break;
        };
}

static void dump_addrs_callback(struct mptcpd_addr_info const *info,
                                void *callback_data)
{
        char addrstr[INET6_ADDRSTRLEN];  // Long enough for both IPv4
                                         // and IPv6 addresses.

        struct mptcpd_idm *const idm = callback_data;

        /**
         * @todo The user would have to perform a similar cast when
         *       retrieving the @c sockaddr.  Perhaps we should export
         *       a new set of @c mptcpd_addr_info_get_foo() functions
         *       for each of the @c mptcpd_addr_info fields.
         */
        struct sockaddr const *const sa =
                (struct sockaddr const *) &info->addr;

        void const *src = NULL;
        if (sa->sa_family == AF_INET)
                src = &((struct sockaddr_in  const *) sa)->sin_addr;
        else
                src = &((struct sockaddr_in6 const *) sa)->sin6_addr;

        (void) inet_ntop(sa->sa_family,
                         src,
                         addrstr,
                         sizeof(addrstr));

        if (mptcpd_idm_map_id(idm,
                              sa,
                              info->id))
                l_debug("ID sync: %u | %s", info->id, addrstr);
        else
                l_error("ID sync failed: %u | %s", info->id, addrstr);
}

static void complete_pm_init(struct mptcpd_pm *pm)
{
        /*
          Register callbacks for MPTCP generic netlink multicast
          notifications.
        */
        pm->id = l_genl_family_register(pm->family,
                                        pm->netlink_pm->group,
                                        handle_mptcp_event,
                                        pm,
                                        NULL /* destroy */);

        if (pm->id == 0) {
                /**
                 * @todo Should we exit with an error instead?
                 */
                l_warn("Unable to register handler for %s "
                       "multicast messages",
                       pm->netlink_pm->group);
        }

        /*
          MPTCP address IDs may already be assigned prior to mptcpd
          start by other applications or previous runs of mptcpd.
          Synchronize mptcpd address ID manager with address IDs
          maintained by the kernel.
         */
        if (pm->netlink_pm->cmd_ops->dump_addrs != NULL
            && pm->netlink_pm->cmd_ops->dump_addrs(pm,
                                                   dump_addrs_callback,
                                                   pm->idm) != 0)
                l_error("Unable to synchronize ID manager with kernel.");

        /**
         * @todo Register a callback once the kernel MPTCP path
         *       management generic netlink API supports
         *       new/removed address notifications so that the mptcpd
         *       ID manager state can be synchronized with the kernel
         *       dynamically.
         */

        /**
         * @bug Mptcpd plugins should only be loaded once at process
         *      start.  The @c mptcpd_plugin_load() function only
         *      loads the functions once, and only reloads after
         *      @c mptcpd_plugin_unload() is called.
         */
        if (!mptcpd_plugin_load(pm->config->plugin_dir,
                                pm->config->default_plugin,
                                pm)) {
                l_error("Unable to load path manager plugins.");

                mptcpd_pm_destroy(pm);

                exit(EXIT_FAILURE);
        }
}

static void notify_pm_ready(void *data, void *user_data)
{
        struct pm_ops_info         *const info = data;
        struct mptcpd_pm_ops const *const ops  = info->ops;
        struct mptcpd_pm           *const pm   = user_data;

        if (ops->ready)
                ops->ready(pm, info->user_data);
}

static void notify_pm_not_ready(void *data, void *user_data)
{
        struct pm_ops_info         *const info = data;
        struct mptcpd_pm_ops const *const ops  = info->ops;
        struct mptcpd_pm           *const pm   = user_data;

        if (ops->not_ready)
                ops->not_ready(pm, info->user_data);
}

/**
 * @brief Handle MPTCP generic netlink family appearing on us.
 *
 * This function performs operations that must occur after the MPTCP
 * generic netlink family has appeared since some data is only
 * available after that has happened.  Such data includes multicast
 * groups exposed by the generic netlink family, etc.
 *
 * @param[in]     info      Generic netlink family information.
 * @param[in,out] user_data Pointer to @c mptcp_pm object to which the
 *                          @c l_genl_family watch belongs.
 */
static void family_appeared(struct l_genl_family_info const *info,
                            void *user_data)
{
        if (info == NULL) {
                /*
                  Request for "mptcp" generic netlink family failed.
                  Wait for it to appear in case it is loaded into the
                  kernel after mptcpd has started.
                */

                l_debug("Request for MPTCP generic netlink "
                        "family failed. Waiting.");

                return;
        }

        char const *const name = l_genl_family_info_get_name(info);

        l_debug("\"%s\" generic netlink family appeared", name);

        struct mptcpd_pm *const pm = user_data;

        /*
          This function could be called in either of two cases, (1)
          handling the appearance of the MPTCP generic netlink family
          through a family watch, or (2) through an explicit family
          request.

          Do nothing if the necessary MPTCP family registration was
          completed as a result of a previous call to this function.
         */
        if (pm->family != NULL)
                return;

        pm->family = l_genl_family_new(pm->genl, name);

        complete_pm_init(pm);

        l_queue_foreach(pm->event_ops, notify_pm_ready, pm);
}

/**
 * @brief Handle MPTCP generic netlink family disappearing on us.
 *
 * @param[in]     name      Generic netlink family name.
 * @param[in,out] user_data Pointer to @c mptcp_pm object to which the
 *                          @c l_genl_family watch belongs.
 */
static void family_vanished(char const *name, void *user_data)
{
        struct mptcpd_pm *const pm = user_data;

        l_debug("%s generic netlink family vanished", name);

        /*
          Unregister callbacks for MPTCP generic netlink multicast
          notifications.
        */
        if (pm->id != 0) {
                if (!l_genl_family_unregister(pm->family, pm->id))
                        l_warn("MPTCP event handler deregistration "
                               "failed.");

                pm->id = 0;
        }

        l_genl_family_free(pm->family);
        pm->family = NULL;

        // Re-arm the MPTCP generic netlink family timeout.
        l_timeout_modify(pm->timeout, FAMILY_TIMEOUT_SECONDS);

        l_queue_foreach(pm->event_ops, notify_pm_not_ready, pm);
}

/**
 * @brief Handle MPTCP generic netlink family appearance timeout.
 *
 * @param[in] timeout   Timeout information.
 * @param[in] user_data Pointer to @c mptcp_pm object to which the
 *                      timeout belongs.
 */
static void family_timeout(struct l_timeout *timeout, void *user_data)
{
        (void) timeout;

        struct mptcpd_pm const *const pm =  user_data;

        if (pm->family != NULL)
                return;  // MPTCP genl family appeared.

        l_warn("MPTCP generic netlink family has not appeared.");
        l_warn("Verify MPTCP netlink path manager kernel support.");
}

static struct mptcpd_nm_ops const _nm_ops = {
        .new_interface    = mptcpd_plugin_new_interface,
        .update_interface = mptcpd_plugin_update_interface,
        .delete_interface = mptcpd_plugin_delete_interface,
        .new_address      = mptcpd_plugin_new_local_address,
        .delete_address   = mptcpd_plugin_delete_local_address,
};

struct mptcpd_pm *mptcpd_pm_create(struct mptcpd_config const *config)
{
        assert(config != NULL);

        struct mptcpd_netlink_pm const *netlink_pm =
                mptcpd_get_netlink_pm();

        if (netlink_pm == NULL) {
                l_error("Required kernel MPTCP support not available.");
                return NULL;
        }

        struct mptcpd_pm *const pm = l_new(struct mptcpd_pm, 1);

        // No need to check for NULL.  l_new() abort()s on failure.

        pm->config     = config;
        pm->netlink_pm = netlink_pm;

        pm->genl = l_genl_new();
        if (pm->genl == NULL) {
                mptcpd_pm_destroy(pm);
                l_error("Unable to initialize Generic Netlink system.");
                return NULL;
        }

        if (l_genl_add_family_watch(pm->genl,
                                    pm->netlink_pm->name,
                                    family_appeared,
                                    family_vanished,
                                    pm,
                                    NULL) == 0
            || !l_genl_request_family(pm->genl,
                                      pm->netlink_pm->name,
                                      family_appeared,
                                      pm,
                                      NULL)) {
                mptcpd_pm_destroy(pm);
                l_error("Unable to watch or request \"%s\" "
                        "generic netlink family.",
                        pm->netlink_pm->name);
                return NULL;
        }

        /*
          Warn the user if the MPTCP generic netlink family doesn't
          appear within a reasonable amount of time.
        */
        pm->timeout =
                l_timeout_create(FAMILY_TIMEOUT_SECONDS,
                                 family_timeout,
                                 pm,
                                 NULL);

        if (pm->timeout == NULL) {
                mptcpd_pm_destroy(pm);
                l_error("Unable to create timeout handler.");
                return NULL;
        }

        // Listen for network device changes.
        pm->nm = mptcpd_nm_create(config->notify_flags);

        if (pm->nm == NULL
            || !mptcpd_nm_register_ops(pm->nm, &_nm_ops, pm)) {
                mptcpd_pm_destroy(pm);
                l_error("Unable to create network monitor.");
                return NULL;
        }

        // Create mptcpd address ID manager.
        pm->idm = mptcpd_idm_create();

        if (pm->idm == NULL) {
                mptcpd_pm_destroy(pm);
                l_error("Unable to create ID manager.");
                return NULL;
        }

        pm->event_ops = l_queue_new();

        return pm;
}

void mptcpd_pm_destroy(struct mptcpd_pm *pm)
{
        if (pm == NULL)
                return;

        /**
         * @bug Mptcpd plugins should only be unloaded once at process
         *      exit, or at least after the last @c mptcpd_pm object
         *      has been destroyed.
         */
        mptcpd_plugin_unload(pm);

        l_queue_destroy(pm->event_ops, l_free);
        mptcpd_idm_destroy(pm->idm);
        mptcpd_nm_destroy(pm->nm);
        l_timeout_remove(pm->timeout);
        l_genl_family_free(pm->family);
        l_genl_unref(pm->genl);
        l_free(pm);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
