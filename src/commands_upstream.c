// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file commands_upstream.c
 *
 * @brief Upstream kernel MPTCP generic netlink commands.
 *
 * Copyright (c) 2020, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#include <assert.h>
#include <errno.h>
#include <sys/socket.h>

#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error(), etc.
#include <ell/log.h>

#include <mptcpd/mptcp_private.h>
#include <mptcpd/path_manager_private.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/types.h>
#include <mptcpd/addr_info.h>

#include "commands.h"
#include "path_manager.h"


/**
 * @struct dumped_addrs
 *
 * @brief Convenience struct for passing more than one addr info.
 */
struct dumped_addrs
{
        /// Array of @c struct @c mptcpd_addr_info objects.
        struct mptcpd_addr_info *addrs;

        /// Length of the above array.
        size_t len;
};

/**
 * @struct retrieved_limits
 *
 * @brief Convenience struct for passing more than one limit.
 */
struct retrieved_limits
{
        /// Array of @c struct @c mptcpd_limit objects.
        struct mptcpd_limit *limits;

        /// Length of the above array.
        size_t len;
};

// -----------------------------------------------------------------------

static void get_addr_callback_recurse(struct l_genl_attr *attr,
                                      struct mptcpd_addr_info *info)
{
        struct l_genl_attr nested;
        if (!l_genl_attr_recurse(attr, &nested)) {
                l_error("get_addr: unable to obtain nested data");
                return;
        }

        uint16_t type;
        uint16_t len;
        void const *data = NULL;

        struct in_addr  const *addr4 = NULL;
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
                        addr4 = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_ADDR6:
                        addr6 = data;
                        break;
                case MPTCP_PM_ADDR_ATTR_PORT:
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
}

static void get_addr_callback(struct l_genl_msg *msg, void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
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

        struct dumped_addrs *const dump = user_data;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                if (type != MPTCP_PM_ATTR_ADDR) {
                        // Sanity check.  This condition should never occur.
                        l_error("get_addr: unexpected data");
                        continue;
                }

                size_t const offset = dump->len++;

                dump->addrs =
                        l_realloc(dump->addrs,
                                  sizeof(*dump->addrs) * dump->len);

                get_addr_callback_recurse(&attr, dump->addrs + offset);
        }
}

static bool append_addr_attr(struct l_genl_msg *msg,
                             struct sockaddr const *addr)
{
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

// --------------------------------------------------------------

static void get_limits_callback(struct l_genl_msg *msg, void *user_data)
{
        struct l_genl_attr attr;
        if (!l_genl_attr_init(&attr, msg)) {
                l_error("Unable to initialize genl attribute");
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

        struct retrieved_limits *const rl = user_data;

        while (l_genl_attr_next(&attr, &type, &len, &data)) {
                size_t const offset = rl->len++;

                rl->limits = l_realloc(rl->limits,
                                       sizeof(*rl->limits) * rl->len);

                struct mptcpd_limit *const l = rl->limits + offset;

                l->type = type;
                l->limit = *(uint32_t const *) data;
        }
}

// --------------------------------------------------------------

static int upstream_add_addr(struct mptcpd_pm *pm,
                             struct sockaddr const *addr,
                             mptcpd_aid_t address_id,
                             uint32_t flags,
                             int index,
                             mptcpd_token_t token)
{
        /*
          MPTCP connection token is not currently needed by upstream
          kernel to add network address.
        */
        if (token != 0)
                l_warn("add_addr: MPTCP connection token is ignored.");

        /*
          Payload (nested):
              Local address family
              Local address
              Local address ID (optional)
              Flags (optional)
              Network inteface index (optional)
         */

        // Types chosen to match MPTCP genl API.
        uint16_t const family = mptcpd_get_addr_family(addr);

        size_t const addr_size = mptcpd_get_addr_size(addr);

        size_t const payload_size =
                MPTCPD_NLA_ALIGN(family)
                + MPTCPD_NLA_ALIGN(addr_size)
                + MPTCPD_NLA_ALIGN_OPT(address_id)
                + MPTCPD_NLA_ALIGN_OPT(flags)
                + MPTCPD_NLA_ALIGN_OPT(index);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_ADD_ADDR, payload_size);

        bool const appended =
                l_genl_msg_enter_nested(msg, MPTCP_PM_ATTR_ADDR)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_PM_ADDR_ATTR_FAMILY,
                        sizeof(family),  // sizeof(uint16_t)
                        &family)
                && append_addr_attr(msg, addr)
                && (address_id == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_ID,
                            sizeof(address_id),  // sizeof(uint8_t)
                            &address_id))
                && (flags == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_FLAGS,
                            sizeof(flags),  // sizeof(uint32_t)
                            &flags))
                && (index == 0
                    || l_genl_msg_append_attr(
                            msg,
                            MPTCP_PM_ADDR_ATTR_IF_IDX,
                            sizeof(index),   // sizeof(int32_t)
                            &index))
                && l_genl_msg_leave_nested(msg);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int upstream_remove_addr(struct mptcpd_pm *pm,
                                mptcpd_aid_t address_id,
                                mptcpd_token_t token)
{
        /*
          MPTCP connection token is not currently needed by upstream
          kernel to remove network address.
        */
        if (token != 0)
                l_warn("remove_addr: MPTCP connection token "
                       "is ignored.");

        /*
          Payload (nested):
                  Local address ID
         */

        size_t const payload_size = MPTCPD_NLA_ALIGN(address_id);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_PM_CMD_DEL_ADDR, payload_size);

        bool const appended =
                l_genl_msg_enter_nested(msg, MPTCP_PM_ATTR_ADDR)
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
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int upstream_get_addr(struct mptcpd_pm *pm,
                             mptcpd_aid_t address_id,
                             struct mptcpd_addr_info **addr)
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
                l_genl_msg_enter_nested(msg, MPTCP_PM_ATTR_ADDR)
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

        struct dumped_addrs dump = { .addrs = NULL };

        /**
         * @todo This is assumed to be a synchronous call.  Verify.
         *       If not, we'll likely need to manage the addr memory
         *       differently.
         */
        int const request_id =
                l_genl_family_send(pm->family,
                                   msg,
                                   get_addr_callback,
                                   &dump, /* user data */
                                   NULL  /* destroy */);

        if (request_id != 0) {
                *addr = dump.addrs;
                assert(dump.len == 1);

                return 0;
        }

        return -1;
}

static int upstream_dump_addrs(struct mptcpd_pm *pm,
                               struct mptcpd_addr_info **addrs,
                               size_t *len)
{
        /*
          Payload:
              NONE
         */

        struct l_genl_msg *const msg =
                l_genl_msg_new(MPTCP_PM_CMD_GET_ADDR);

        struct dumped_addrs dump = { .addrs = NULL };

        int const request_id =
                l_genl_family_dump(pm->family,
                                   msg,
                                   get_addr_callback,
                                   &dump, /* user data */
                                   NULL  /* destroy */);

        if (request_id != 0) {
                *addrs = dump.addrs;
                *len = dump.len;

                return 0;
        }

        return -1;
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
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                == 0;
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
                if (!l_genl_msg_append_attr(msg,
                                            l->type,
                                            sizeof(l->limit),
                                            &l->limit)) {
                        l_genl_msg_unref(msg);

                        return ENOMEM;
                }
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int upstream_get_limits(struct mptcpd_pm *pm,
                               struct mptcpd_limit **limits,
                               size_t *len)
{
        /*
          Payload:
              NONE
         */

        struct l_genl_msg *const msg =
                l_genl_msg_new(MPTCP_PM_CMD_GET_ADDR);

        struct retrieved_limits rl = { .limits = NULL };

        int const request_id =
                l_genl_family_dump(pm->family,
                                   msg,
                                   get_limits_callback,
                                   &rl, /* user data */
                                   NULL  /* destroy */);

        if (request_id != 0) {
                *limits = rl.limits;
                *len = rl.len;

                return 0;
        }

        return -1;
}

static struct mptcpd_pm_cmd_ops const cmd_ops =
{
        .add_addr    = upstream_add_addr,
        .remove_addr = upstream_remove_addr,
        .get_addr    = upstream_get_addr,
        .dump_addrs  = upstream_dump_addrs,
        .flush_addrs = upstream_flush_addrs,
        .set_limits  = upstream_set_limits,
        .get_limits  = upstream_get_limits
};

struct mptcpd_pm_cmd_ops const *mptcpd_get_upstream_cmd_ops(void)
{
        return &cmd_ops;
}

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
