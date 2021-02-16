// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file commands_mptcp_org.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <errno.h>

#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error(), etc.
#include <ell/log.h>

#include "commands.h"

#include <mptcpd/mptcp_private.h>
#include <mptcpd/private/path_manager.h>
#include <mptcpd/path_manager.h>


static uint16_t get_port_number(struct sockaddr const *addr)
{
        in_port_t port = 0;

        if (addr == NULL)
                return port;

        if (addr->sa_family == AF_INET) {
                struct sockaddr_in const *const addr4 =
                        (struct sockaddr_in const*) addr;

                port = addr4->sin_port;

        } else if (addr->sa_family == AF_INET6) {
                struct sockaddr_in6 const *const addr6 =
                        (struct sockaddr_in6 const*) addr;

                port = addr6->sin6_port;
        }

        return port;
}

static bool append_addr_attr(struct l_genl_msg *msg,
                             struct sockaddr const *addr,
                             bool local)
{
        assert(mptcpd_is_inet_family(addr));

        uint16_t type = 0;
        uint16_t len = 0;
        void const *data = NULL;

        if (addr->sa_family == AF_INET) {
                if (local)
                        type = MPTCP_ATTR_SADDR4;
                else
                        type = MPTCP_ATTR_DADDR4;

                struct sockaddr_in const *const addr4 =
                        (struct sockaddr_in *) addr;

                data = &addr4->sin_addr;
                len  = sizeof(addr4->sin_addr);
        } else {
                if (local)
                        type = MPTCP_ATTR_SADDR6;
                else
                        type = MPTCP_ATTR_DADDR6;

                struct sockaddr_in6 const *const addr6 =
                        (struct sockaddr_in6 *) addr;

                data = &addr6->sin6_addr;
                len  = sizeof(addr6->sin6_addr);
        }

        return l_genl_msg_append_attr(msg, type, len, data);
}

static bool append_local_addr_attr(struct l_genl_msg *msg,
                                   struct sockaddr const *addr)
{
        static bool const local = true;

        return append_addr_attr(msg, addr, local);
}

static bool append_remote_addr_attr(struct l_genl_msg *msg,
                                    struct sockaddr const *addr)
{
        static bool const local = false;

        return append_addr_attr(msg, addr, local);
}

// ---------------------------------------------------------------------

static int mptcp_org_add_addr(struct mptcpd_pm *pm,
                              struct sockaddr const *addr,
                              mptcpd_aid_t id,
                              uint32_t const flags,
                              int index,
                              mptcpd_token_t token)
{
        /*
          MPTCP flags are not needed by multipath-tcp.org kernel to
          add network address.
        */
        if (flags != 0)
                l_warn("add_addr: MPTCP flags are ignored.");

        /*
          Network interface index is not needed by multipath-tcp.org
          kernel to add network address.
        */
        if (index != 0)
                l_warn("add_addr: Network interface index is ignored.");

        /*
          Payload:
              Token
              Local address ID
              Local address family
              Local address
              Local port (optional)
         */

        // Types chosen to match MPTCP genl API.
        uint16_t const family = mptcpd_get_addr_family(addr);
        uint16_t const port   = get_port_number(addr);

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(id)
                + MPTCPD_NLA_ALIGN(family)
                + NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(addr))
                + MPTCPD_NLA_ALIGN_OPT(port);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_CMD_ANNOUNCE, payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_LOC_ID,
                        sizeof(id),
                        &id)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_FAMILY,
                        sizeof(family),  // sizeof(uint16_t)
                        &family)
                && append_local_addr_attr(msg, addr)
                && (port == 0 ||
                    l_genl_msg_append_attr(msg,
                                           MPTCP_ATTR_SPORT,
                                           sizeof(port),
                                           &port));

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "add_addr", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int mptcp_org_remove_addr(struct mptcpd_pm *pm,
                                 mptcpd_aid_t address_id,
                                 mptcpd_token_t token)
{
        /*
          Payload:
              Token
              Local address ID
         */

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(address_id);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_CMD_REMOVE, payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_LOC_ID,
                        sizeof(address_id),
                        &address_id);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "remove_addr", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int mptcp_org_add_subflow(struct mptcpd_pm *pm,
                                 mptcpd_token_t token,
                                 mptcpd_aid_t local_address_id,
                                 mptcpd_aid_t remote_address_id,
                                 struct sockaddr const *local_addr,
                                 struct sockaddr const *remote_addr,
                                 bool backup)
{
        /*
          Payload:
              Token
              Address family
              Local address ID
              Remote address ID

              Optional attributes:
                  Local address
                  Local port
                  Remote address
                  Remote port    (required if remote address is specified)
                  Backup priority flag
                  Network interface index
         */

        /**
         * @bug The address family is required but we currently base
         *      it on the optional remote address.
         */
        uint16_t const family      = mptcpd_get_addr_family(remote_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(family)
                + MPTCPD_NLA_ALIGN(local_address_id)
                + MPTCPD_NLA_ALIGN(remote_address_id)
                + (local_addr == NULL
                   ? 0
                   : (NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(local_addr))
                      + (MPTCPD_NLA_ALIGN_OPT(local_port))))
                + (remote_addr == NULL
                   ? 0
                   : NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(remote_addr))
                   + MPTCPD_NLA_ALIGN(remote_port))
                + MPTCPD_NLA_ALIGN(backup);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_CMD_SUB_CREATE, payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_LOC_ID,
                        sizeof(local_address_id),
                        &local_address_id)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_REM_ID,
                        sizeof(remote_address_id),
                        &remote_address_id)
                && (local_addr == NULL
                    || (append_local_addr_attr(msg, local_addr)
                        && (local_port == 0
                            || l_genl_msg_append_attr(msg,
                                                      MPTCP_ATTR_SPORT,
                                                      sizeof(local_port),
                                                      &local_port))))
                && (remote_addr == NULL
                    || (l_genl_msg_append_attr(
                                msg,
                                MPTCP_ATTR_FAMILY,
                                sizeof(family),
                                &family)
                        && append_remote_addr_attr(msg, remote_addr)
                        && remote_port != 0
                        && l_genl_msg_append_attr(msg,
                                                  MPTCP_ATTR_DPORT,
                                                  sizeof(remote_port),
                                                  &remote_port)))
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_BACKUP,
                                          sizeof(backup),
                                          &backup);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "add_subflow", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int mptcp_org_set_backup(struct mptcpd_pm *pm,
                                mptcpd_token_t token,
                                struct sockaddr const *local_addr,
                                struct sockaddr const *remote_addr,
                                bool backup)
{
        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
              Backup priority flag
         */

        uint16_t const family      = mptcpd_get_addr_family(local_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(family)
                + NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(local_addr))
                + MPTCPD_NLA_ALIGN(local_port)
                + NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(remote_addr))
                + MPTCPD_NLA_ALIGN(remote_port)
                + MPTCPD_NLA_ALIGN(backup);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_CMD_SUB_PRIORITY,
                                     payload_size);

        /**
         * @todo Should we verify that the local and remote address
         *       families match?  The kernel already does that, but we
         *       currently don't propagate any kernel initiated errors
         *       to the caller.
         */
        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_FAMILY,
                        sizeof(family),
                        &family)
                && append_local_addr_attr(msg, local_addr)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_SPORT,
                                          sizeof(local_port),
                                          &local_port)
                && append_remote_addr_attr(msg, remote_addr)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_DPORT,
                                          sizeof(remote_port),
                                          &remote_port)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_BACKUP,
                                          sizeof(backup),
                                          &backup);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "set_backup", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static int mptcp_org_remove_subflow(struct mptcpd_pm *pm,
                                    mptcpd_token_t token,
                                    struct sockaddr const *local_addr,
                                    struct sockaddr const *remote_addr)
{
        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
         */

        uint16_t const family      = mptcpd_get_addr_family(local_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                MPTCPD_NLA_ALIGN(token)
                + MPTCPD_NLA_ALIGN(family)
                + NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(local_addr))
                + MPTCPD_NLA_ALIGN(local_port)
                + NLA_HDRLEN + NLA_ALIGN(mptcpd_get_addr_size(remote_addr))
                + MPTCPD_NLA_ALIGN(remote_port);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_CMD_SUB_DESTROY, payload_size);

        /**
         * @todo Should we verify that the local and remote address
         *       families match?  The kernel already does that, but we
         *       currently don't propagate any kernel initiated errors
         *       to the caller.
         */
        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_FAMILY,
                        sizeof(family),
                        &family)
                && append_local_addr_attr(msg, local_addr)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_SPORT,
                                          sizeof(local_port),
                                          &local_port)
                && append_remote_addr_attr(msg, remote_addr)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_DPORT,
                                          sizeof(remote_port),
                                          &remote_port);

        if (!appended) {
                l_genl_msg_unref(msg);

                return ENOMEM;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  mptcpd_family_send_callback,
                                  "remove_subflow", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

static struct mptcpd_pm_cmd_ops const cmd_ops =
{
        .add_addr       = mptcp_org_add_addr,
        .remove_addr    = mptcp_org_remove_addr,
        .add_subflow    = mptcp_org_add_subflow,
        .remove_subflow = mptcp_org_remove_subflow,
        .set_backup     = mptcp_org_set_backup,
};

struct mptcpd_pm_cmd_ops const *mptcpd_get_mptcp_org_cmd_ops(void)
{
        return &cmd_ops;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
