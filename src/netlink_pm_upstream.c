// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file netlink_pm_upstream.c
 *
 * @brief Upstream kernel generic netlink path manager details.
 *
 * Copyright (c) 2020-2022, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error(), etc.
#include <ell/log.h>
#pragma GCC diagnostic pop

#include <mptcpd/types.h>
#include <mptcpd/listener_manager.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/private/netlink_pm.h>
#include <mptcpd/private/path_manager.h>
#include <mptcpd/private/addr_info.h>
#include <mptcpd/private/sockaddr.h>
#include <mptcpd/private/mptcp_upstream.h>

#include "commands.h"
#include "netlink_pm.h"
#include "path_manager.h"

// Sanity check
#if MPTCPD_ADDR_FLAG_SIGNAL != MPTCP_PM_ADDR_FLAG_SIGNAL                \
        || MPTCPD_ADDR_FLAG_SUBFLOW != MPTCP_PM_ADDR_FLAG_SUBFLOW       \
        || MPTCPD_ADDR_FLAG_BACKUP != MPTCP_PM_ADDR_FLAG_BACKUP
# error Mismatch between mptcpd and upstream kernel addr flags.
#endif


// --------------------------------------------------------------
//                 Common Utility Functions
// --------------------------------------------------------------
/**
 * @brief Initialize a @c struct @c mptcpd_addr_info instance.
 *
 * Initialize a @c addr_info instance with the provided IPv4 or
 * IPv6 address.  Only one is required and used.  The @a port, @a id,
 * @a flags, and @a index are optional and may be set to @c NULL if
 * not used.
 *
 * @param[in]     addr4 IPv4 internet address.
 * @param[in]     addr6 IPv6 internet address.
 * @param[in]     port  IP port.
 * @param[in]     id    Address ID.
 * @param[in]     flags MPTCP flags.
 * @param[in]     index Network interface index.
 * @param[in,out] addr  mptcpd network address information.
 *
 * @note This function is mostly meant for internal use.
 *
 * @return @c true on success.  @c false otherwise.
 */
static bool mptcpd_addr_info_init(in_addr_t       const *addr4,
                                  struct in6_addr const *addr6,
                                  in_port_t       const *port,
                                  uint8_t         const *id,
                                  uint32_t        const *flags,
                                  int32_t         const *index,
                                  struct mptcpd_addr_info *info)
{
        if (info == NULL
            || !mptcpd_sockaddr_storage_init(addr4,
                                             addr6,
                                             port ? *port : 0,
                                             &info->addr))
                return false;

        info->id    = (id    ? *id    : 0);
        info->flags = (flags ? *flags : 0);
        info->index = (index ? *index : 0);

        return true;
}

struct addr_info
{
        struct sockaddr const *const addr;
        mptcpd_aid_t id;
        uint32_t flags;
        int32_t ifindex;
};

static bool append_ip(struct l_genl_msg *msg, struct addr_info *info)
{
        struct sockaddr const *const addr = info->addr;

        assert(mptcpd_is_inet_family(addr));

        uint16_t type = 0;
        uint16_t len = 0;
        void const *data = NULL;

        if (addr->sa_family == AF_INET) {
                type = MPTCP_PM_ADDR_ATTR_ADDR4;

                struct sockaddr_in const *const addr4 =
                        (struct sockaddr_in *) addr;

                data = &addr4->sin_addr;
                len  = sizeof(addr4->sin_addr);
        } else {
                type = MPTCP_PM_ADDR_ATTR_ADDR6;

                struct sockaddr_in6 const *const addr6 =
                        (struct sockaddr_in6 *) addr;

                data = &addr6->sin6_addr;
                len  = sizeof(addr6->sin6_addr);
        }

        return l_genl_msg_append_attr(msg, type, len, data);
}

static bool append_addr_attr(struct l_genl_msg *msg,
                             struct addr_info *info,
                             uint16_t nested_type)
{
        assert(nested_type == MPTCP_PM_ATTR_ADDR
               || nested_type == MPTCP_PM_ATTR_ADDR_REMOTE);

        // Types chosen to match MPTCP genl API.
        uint16_t const family = mptcpd_get_addr_family(info->addr);
        uint16_t const port   = mptcpd_get_port_number(info->addr);

        return l_genl_msg_enter_nested(msg,
                                       NLA_F_NESTED | nested_type)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ADDR_ATTR_FAMILY,
                        sizeof(family),  // sizeof(uint16_t)
                        &family)
                && append_ip(msg, info)
                && (port == 0 ||
                    l_genl_msg_append_attr(msg,
                                           MPTCP_PM_ADDR_ATTR_PORT,
                                           sizeof(port),  // sizeof(uint16_t)
                                           &port))
                && (info->id == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_ID,
                            sizeof(info->id),  // sizeof(uint8_t)
                            &info->id))
                && (info->flags == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_FLAGS,
                            sizeof(info->flags),  // sizeof(uint32_t)
                            &info->flags))
                && (info->ifindex == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_IF_IDX,
                            sizeof(info->ifindex),   // sizeof(int32_t)
                            &info->ifindex))
                && l_genl_msg_leave_nested(msg);
}

static bool append_local_addr_attr(struct l_genl_msg *msg,
                                   struct addr_info *info)
{
        static uint16_t const nested_type = MPTCP_PM_ATTR_ADDR;

        return append_addr_attr(msg, info, nested_type);
}

static bool append_remote_addr_attr(struct l_genl_msg *msg,
                                    struct addr_info *info)
{
        static uint16_t const nested_type = MPTCP_PM_ATTR_ADDR_REMOTE;

        return append_addr_attr(msg, info, nested_type);
}

static int send_add_addr(struct mptcpd_pm *pm,
                         uint8_t cmd,
                         char const *cmd_name,
                         struct addr_info *info,
                         mptcpd_token_t token)
{
        assert(cmd == MPTCP_PM_CMD_ANNOUNCE
               || cmd == MPTCP_PM_CMD_ADD_ADDR);

        /*
          Payload (nested):
              Local address family
              Local address
              Local port (optional)
              Local address ID (optional)
              Token (required for user space MPTCP_PM_CMD_ANNOUNCE)
              Flags (optional)
              Network inteface index (optional)
         */

        // Types chosen to match MPTCP genl API.
        uint16_t const family = mptcpd_get_addr_family(info->addr);
        uint16_t const port   = mptcpd_get_port_number(info->addr);

        /*
          The MPTCP_PM_ADDR_FLAG_SIGNAL flag is required when a port
          is specified.  Make sure it is set.
        */
        if (port != 0)
                info->flags |= MPTCP_PM_ADDR_FLAG_SIGNAL;

        size_t const payload_size =
                MPTCPD_NLA_ALIGN(family)
                + MPTCPD_NLA_ALIGN_ADDR(info->addr)
                + MPTCPD_NLA_ALIGN_OPT(port)
                + MPTCPD_NLA_ALIGN_OPT(info->id)
                + MPTCPD_NLA_ALIGN_OPT(info->flags)
                + MPTCPD_NLA_ALIGN_OPT(info->ifindex)
                + MPTCPD_NLA_ALIGN_OPT(token);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd, payload_size);

        bool const appended =
                append_local_addr_attr(msg, info)
                && (token == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ATTR_TOKEN,
                            sizeof(token),  // sizeof(uint32_t)
                            &token));

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  (void *) cmd_name, /* user data */
                                  NULL  /* destroy */) == 0;
}

// --------------------------------------------------------------
//          User Space Path Manager Related Functions
// --------------------------------------------------------------
static int upstream_announce(struct mptcpd_pm *pm,
                             struct sockaddr const *addr,
                             mptcpd_aid_t id,
                             mptcpd_token_t token)
{
        /**
         * @todo Add support for the optional network interface index
         *       attribute.
         */
        struct addr_info info = {
                .addr     = addr,
                .id       = id,
                .flags    = MPTCP_PM_ADDR_FLAG_SIGNAL,
                // .ifindex  = ...
        };

        /**
         * Set up MPTCP listening socket.
         *
         * @todo This should be optional.
         */
        if (!mptcpd_lm_listen(pm->lm, addr))
                return -1;

        return send_add_addr(pm,
                             MPTCP_PM_CMD_ANNOUNCE,
                             "announce",
                             &info,
                             token);
}

struct remove_info
{
        struct mptcpd_lm *const lm;
        struct sockaddr const *const sa;
};

static void upstream_remove_callback(struct l_genl_msg *msg, void *user_data)
{
        static char const op[] = "remove_addr";

        mptcpd_family_send_callback(msg, (void *) op);

        /**
         * @todo The above @c mptcpd_family_send_callback() function
         *       also calls @c l_genl_msg_get_error().  We could
         *       refactor but that may not be worth the trouble since
         *       @c l_genl_msg_get_error() is not an expensive call.
         */
        if (l_genl_msg_get_error(msg) == 0) {
                struct remove_info *info = user_data;

                /**
                 * Stop listening on MPTCP socket.
                 *
                 * @todo This should be optional.
                 */
                (void) mptcpd_lm_close(info->lm, info->sa);
        }
}

static int upstream_remove(struct mptcpd_pm *pm,
                           struct sockaddr const *addr,
                           mptcpd_aid_t id,
                           mptcpd_token_t token)
{
        /**
         * @todo Refactor upstream_remove() and
         *       mptcp_org_remove_addr() functions. They only differ
         *       by command and attribute types.
         */

        /*
          Payload:
              Token
              Local address ID
         */

        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(id);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_REMOVE, payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_PM_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ATTR_LOC_ID,
                        sizeof(id),
                        &id);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        struct remove_info info = { .lm = pm->lm, .sa = addr };

        bool const result =
                l_genl_family_send(pm->family,
                                   msg,
                                   upstream_remove_callback,
                                   &info, /* user data */
                                   NULL  /* destroy */);

        return result == 0;
}

static int upstream_add_subflow(struct mptcpd_pm *pm,
                                mptcpd_token_t token,
                                mptcpd_aid_t local_id,
                                mptcpd_aid_t remote_id,
                                struct sockaddr const *local_addr,
                                struct sockaddr const *remote_addr,
                                bool backup)
{
        (void) remote_id;
        (void) backup;

        /*
          Payload (nested):
              Token
              Local address ID
              Local address family
              Local address
              Remote address family
              Remote address
              Remote port

         */

        /**
         * @todo The local port isn't used.  Should we explicitly set it
         *       to zero, or at least issue a diagnostic if it isn't zero?
         */
        struct addr_info local = {
                .addr = local_addr,
                .id   = local_id
        };

        struct addr_info remote = {
                .addr = remote_addr,
        };

        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(local_id)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // local family
                + MPTCPD_NLA_ALIGN_ADDR(local_addr)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // remote family
                + MPTCPD_NLA_ALIGN_ADDR(remote_addr)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t));  // remote port

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_SUBFLOW_CREATE,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ATTR_TOKEN,
                        sizeof(token),  // sizeof(uint32_t)
                        &token)
                && append_local_addr_attr(msg, &local)
                && append_remote_addr_attr(msg, &remote);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "add_subflow", /* user data */
                                  NULL  /* destroy */) == 0;
}

static int upstream_remove_subflow(struct mptcpd_pm *pm,
                                   mptcpd_token_t token,
                                   struct sockaddr const *local_addr,
                                   struct sockaddr const *remote_addr)
{
        /*
          Payload (nested):
              Token
              Local address family
              Local address
              Local port
              Remote address family
              Remote address
              Remote port

         */

        struct addr_info local = {
                .addr = local_addr,
        };

        struct addr_info remote = {
                .addr = remote_addr,
        };

        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // local family
                + MPTCPD_NLA_ALIGN_ADDR(local_addr)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // local port
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // remote family
                + MPTCPD_NLA_ALIGN_ADDR(remote_addr)
                + MPTCPD_NLA_ALIGN(sizeof(uint16_t));  // remote port

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_SUBFLOW_DESTROY,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ATTR_TOKEN,
                        sizeof(token),  // sizeof(uint32_t)
                        &token)
                && append_local_addr_attr(msg, &local)
                && append_remote_addr_attr(msg, &remote);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "remove_subflow", /* user data */
                                  NULL  /* destroy */) == 0;
}

static int upstream_set_backup(struct mptcpd_pm *pm,
                               mptcpd_token_t token,
                               struct sockaddr const *local_addr,
                               struct sockaddr const *remote_addr,
                               bool backup)
{
        (void) pm;
        (void) token;
        (void) local_addr;
        (void) remote_addr;
        (void) backup;

        return ENOTSUP;
}

// --------------------------------------------------------------
//          Kernel Path Manager Related Functions
// --------------------------------------------------------------
/**
 * @struct get_addr_user_callback
 *
 * @brief Convenience struct for passing addr to user callback.
 */
struct get_addr_user_callback
{
        /// User supplied get_addr/dump_addrs callback.
        mptcpd_kpm_get_addr_cb_t get_addr;

        /// User data to be passed to one of the above callback.
        void *data;

        /**
         * Function to be called upon completion of the
         * get_addr/dump_addrs call.  This function is called
         * regardless of whether or not the get_addr/dump_addrs call
         * asynchronously returns a network address.  It is also only
         * called once, whereas the above @c get_addr callback may be
         * called multiple times during a @c dump_addrs call.
         */
        mptcpd_complete_func_t complete;

        /// Callback is for a dump_addrs call.
        bool dump;
};

/**
 * @struct get_limits_user_callback
 *
 * @brief Convenience struct for passing limits to user callback.
 */
struct get_limits_user_callback
{
        /// User supplied callback.
        mptcpd_pm_get_limits_cb get_limits;

        /// User data to be passed to the above callback.
        void *data;
};

static bool get_addr_callback_recurse(struct l_genl_attr *attr,
                                      struct mptcpd_addr_info *info)
{
        struct l_genl_attr nested;
        if (!l_genl_attr_recurse(attr, &nested)) {
                l_error("get_addr: unable to obtain nested data");
                return false;
        }

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        in_addr_t       const *addr4 = NULL;
        struct in6_addr const *addr6 = NULL;
        in_port_t       const *port  = NULL;
        uint8_t         const *id    = NULL;
        uint32_t        const *flags = NULL;
        int32_t         const *index = NULL;

        while (l_genl_attr_next(&nested, &type, &len, &data)) {
                switch (type) {
                case MPTCP_PM_ADDR_ATTR_FAMILY:
                        /*
                          Ignore.  Deduced from addr attributes
                          below.
                        */
                        break;
                case MPTCP_PM_ADDR_ATTR_ID:
                        id = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_ADDR4:
                        // Sent from kernel in network byte order.
                        addr4 = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_ADDR6:
                        addr6 = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_PORT:
                        // Sent from kernel in host byte order.
                        port = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_FLAGS:
                        flags = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_IF_IDX:
                        index = data;
                        break;
                default:
                        l_warn("Unknown MPTCP_PM_ATTR_ADDR attribute: %d",
                               type);
                        break;
                }
        }

        mptcpd_addr_info_init(addr4, addr6, port, id, flags, index, info);

        return true;
}

static void get_addr_callback(struct l_genl_msg *msg, void *user_data)
{
        struct get_addr_user_callback const *const cb = user_data;

        if (!mptcpd_check_genl_error(msg,
                                     cb->dump
                                     ? "dump_addrs"
                                     : "get_addr"))
                return;

        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("get_addr: "
                        "Unable to initialize genl attribute");

                return;
        }

        /*
          Payload (nested):
              Network address
              Address ID
              Flags
              Network
        */

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        struct mptcpd_addr_info addr = { .id = 0 };

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                /*
                  Sanity check.  The attribute sent by the kernel
                  should always be of type MPTCP_PM_ATTR_ADDR.
                */
                if (type == MPTCP_PM_ATTR_ADDR) {
                        if (!get_addr_callback_recurse(&attr, &addr))
                                return;

                        // Only one addr is sent per get/dump.
                        break;
                } else {
                        /*
                          This should only occur if the kernel
                          get_addr/dump_addrs API changed.
                        */
                        l_error("%s: unexpected attribute of type %u",
                                cb->dump
                                ? "dump_addrs"
                                : "get_addr",
                                type);
                }
        }

        // Pass the results to the user.
        cb->get_addr(&addr, cb->data);
}

static void get_addr_user_callback_free(void *data)
{
        struct get_addr_user_callback *const cb = data;

        if (cb->complete != NULL)
                cb->complete(cb->data);

        l_free(cb);
}

static uint16_t kernel_to_mptcpd_limit(uint16_t type)
{
        // Translate from kernel to mptcpd MPTCP limit.
        switch(type) {
        case MPTCP_PM_ATTR_RCV_ADD_ADDRS:
                return MPTCPD_LIMIT_RCV_ADD_ADDRS;
        case MPTCP_PM_ATTR_SUBFLOWS:
                return MPTCPD_LIMIT_SUBFLOWS;
        default:
                // Kernel sent an unknown MPTCP limit.
                l_warn("Unrecognized MPTCP resource "
                       "limit type: %u.", type);

                break;
        }

        return type;
}

static uint16_t mptcpd_to_kernel_limit(uint16_t type)
{
        switch(type) {
        case MPTCPD_LIMIT_RCV_ADD_ADDRS:
                return MPTCP_PM_ATTR_RCV_ADD_ADDRS;
        case MPTCPD_LIMIT_SUBFLOWS:
                return MPTCP_PM_ATTR_SUBFLOWS;
        default:
                l_warn("Unrecognized MPTCP resource "
                       "limit type: %u.", type);

                break;
        }

        return type;
}

static void get_limits_callback(struct l_genl_msg *msg, void *user_data)
{
        struct get_limits_user_callback const *const cb = user_data;
        struct mptcpd_limit *limits = NULL;
        size_t limits_len = 0;

        if (!mptcpd_check_genl_error(msg, "get_limits"))
                goto get_limits_out;

        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("get_limits: "
                        "Unable to initialize genl attribute");

                goto get_limits_out;
        }

        /*
          Payload (nested):
              Network address
              Address ID
              Flags
              Network
        */

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                size_t const offset = limits_len++;

                limits = l_realloc(limits,
                                   sizeof(*limits) * limits_len);

                struct mptcpd_limit *const l = limits + offset;

                l->type  = kernel_to_mptcpd_limit(type);
                l->limit = *(uint32_t const *) data;
        }

get_limits_out:

        // Pass the results to the user.
        cb->get_limits(limits, limits_len, cb->data);

        l_free(limits);
}

static int upstream_add_addr(struct mptcpd_pm *pm,
                             struct sockaddr const *addr,
                             mptcpd_aid_t id,
                             uint32_t flags,
                             int index)
{
        struct addr_info info = {
                .addr     = addr,
                .id       = id,
                .flags    = flags,
                .ifindex  = index
        };

        static uint32_t const token = 0;  // Unused

        return send_add_addr(pm,
                             MPTCP_PM_CMD_ADD_ADDR,
                             "add_addr",
                             &info,
                             token);
}

static int upstream_remove_addr(struct mptcpd_pm *pm,
                                mptcpd_aid_t address_id)
{
        /*
          Payload (nested):
                  Local address ID
         */

        size_t const payload_size = MPTCPD_NLA_ALIGN(address_id);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_DEL_ADDR, payload_size);

        bool const appended =
                l_genl_msg_enter_nested(msg,
                                        NLA_F_NESTED | MPTCP_PM_ATTR_ADDR)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ADDR_ATTR_ID,
                        sizeof(address_id),
                        &address_id)
                && l_genl_msg_leave_nested(msg);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "remove_addr", /* user data */
                                  NULL  /* destroy */) == 0;
}

static int upstream_get_addr(struct mptcpd_pm *pm,
                             mptcpd_aid_t address_id,
                             mptcpd_kpm_get_addr_cb_t callback,
                             void *data,
                             mptcpd_complete_func_t complete)
{
        /*
          Payload (nested):
              Local address ID
         */

        size_t const payload_size = MPTCPD_NLA_ALIGN(address_id);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_GET_ADDR,
                                     payload_size);

        bool const appended =
                l_genl_msg_enter_nested(msg,
                                        NLA_F_NESTED | MPTCP_PM_ATTR_ADDR)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ADDR_ATTR_ID,
                        sizeof(address_id),
                        &address_id)
                && l_genl_msg_leave_nested(msg);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        struct get_addr_user_callback *const cb =
                l_new(struct get_addr_user_callback, 1);

        cb->get_addr = callback;
        cb->data     = data;
        cb->complete = complete;
        cb->dump     = false;

        return l_genl_family_send(
                pm->family,
                msg,
                get_addr_callback,
                cb,     /* user data */
                get_addr_user_callback_free  /* destroy */) == 0;
}

static int upstream_dump_addrs(struct mptcpd_pm *pm,
                               mptcpd_kpm_get_addr_cb_t callback,
                               void *data,
                               mptcpd_complete_func_t complete)
{
        /*
          Payload:
              NONE
         */

        struct l_genl_msg *const msg =
                l_genl_msg_new(MPTCP_PM_CMD_GET_ADDR);

        struct get_addr_user_callback *const cb =
                l_new(struct get_addr_user_callback, 1);

        cb->get_addr = callback;
        cb->data     = data;
        cb->complete = complete;
        cb->dump     = true;

        return l_genl_family_dump(
                pm->family,
                msg,
                get_addr_callback,
                cb,     /* user data */
                get_addr_user_callback_free  /* destroy */) == 0;
}

static int upstream_flush_addrs(struct mptcpd_pm *pm)
{
        /*
          Payload:
              NONE
         */

        struct l_genl_msg *const msg =
                l_genl_msg_new(MPTCP_PM_CMD_FLUSH_ADDRS);

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "flush_addrs", /* user data */
                                  NULL  /* destroy */) == 0;
}

static int upstream_set_limits(struct mptcpd_pm *pm,
                               struct mptcpd_limit const *limits,
                               size_t len)
{
        if (limits == NULL || len == 0)
                return EINVAL;  // Nothing to set.

        /*
          Payload:
              Maximum number of advertised addresses to receive/accept
              from peer (optional)
              Maximum number of subflows to create (optional)
         */

        size_t const payload_size = len * MPTCPD_NLA_ALIGN(limits->limit);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_SET_LIMITS,
                                     payload_size);

        for (struct mptcpd_limit const *l = limits;
             l != limits + len;
             ++l) {
                uint16_t const type = mptcpd_to_kernel_limit(l->type);

                if (!l_genl_msg_append_attr(msg,
                                            type,
                                            sizeof(l->limit),
                                            &l->limit)) {
                        l_genl_msg_unref(msg);

                        return ENOMEM;
                }
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "set_limits", /* user data */
                                  NULL  /* destroy */) == 0;
}

static int upstream_get_limits(struct mptcpd_pm *pm,
                               mptcpd_pm_get_limits_cb callback,
                               void *data)
{
        /*
          Payload:
              NONE
         */

        struct l_genl_msg *const msg =
                l_genl_msg_new(MPTCP_PM_CMD_GET_LIMITS);

        struct get_limits_user_callback *const cb =
                l_new(struct get_limits_user_callback, 1);

        cb->get_limits = callback;
        cb->data       = data;

        return l_genl_family_send(pm->family,
                                  msg,
                                  get_limits_callback,
                                  cb,     /* user data */
                                  l_free  /* destroy */) == 0;
}

static int upstream_set_flags(struct mptcpd_pm *pm,
                              struct sockaddr const *addr,
                              mptcpd_flags_t flags)
{
        /*
          Payload (nested):
              Local address family
              Local address
              Flags
         */

        struct addr_info info = {
                .addr  = addr,
                .flags = flags
        };

        // Types chosen to match MPTCP genl API.
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(sizeof(uint16_t))  // family
                + MPTCPD_NLA_ALIGN_ADDR(addr)
                + MPTCPD_NLA_ALIGN(flags);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_SET_FLAGS,
                                     payload_size);

        if (!append_local_addr_attr(msg, &info)) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "set_flags", /* user data */
                                  NULL  /* destroy */) == 0;
}

// ---------------------------------------------------------------------

static struct mptcpd_pm_cmd_ops const cmd_ops =
{
        .add_addr       = upstream_announce,
        .remove_addr    = upstream_remove,
        .add_subflow    = upstream_add_subflow,
        .remove_subflow = upstream_remove_subflow,
        .set_backup     = upstream_set_backup,
};

static struct mptcpd_kpm_cmd_ops const kcmd_ops =
{
        .add_addr    = upstream_add_addr,
        .remove_addr = upstream_remove_addr,
        .get_addr    = upstream_get_addr,
        .dump_addrs  = upstream_dump_addrs,
        .flush_addrs = upstream_flush_addrs,
        .set_limits  = upstream_set_limits,
        .get_limits  = upstream_get_limits,
        .set_flags   = upstream_set_flags,
};

static struct mptcpd_netlink_pm const npm = {
        .name     = MPTCP_PM_NAME,
        .group    = MPTCP_PM_EV_GRP_NAME,
        .cmd_ops  = &cmd_ops,
        .kcmd_ops = &kcmd_ops
};

struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void)
{
        static char const path[] = MPTCP_SYSCTL_VARIABLE(enabled);
        static char const name[] = "enabled";
        static int  const enable_val = 1;

        if (!mptcpd_is_kernel_mptcp_enabled(path, name, enable_val))
                return NULL;

        return &npm;
}

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
