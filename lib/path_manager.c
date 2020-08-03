// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file lib/path_manager.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().

#include <assert.h>

#include <netinet/in.h>

#include <linux/netlink.h>
#include LINUX_MPTCP_CLIENT_HEADER

#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>

#include <mptcpd/path_manager.h>
#include <mptcpd/path_manager_private.h>
#include <mptcpd/plugin.h>


static void family_send_callback(struct l_genl_msg *msg, void *user_data)
{
        (void) user_data;

        int const error = l_genl_msg_get_error(msg);

        if (error < 0) {
                // Error during send.  Likely insufficient perms.

                char errmsg[80];
                int const r = strerror_r(-error,
                                         errmsg,
                                         L_ARRAY_SIZE(errmsg));

                l_error("Path manager command error: %s",
                        r == 0 ? errmsg : "<unknown error>");
        }
}

// ---------------------------------------------------------------------

static inline bool is_inet_family(struct sockaddr const *addr)
{
        return addr->sa_family == AF_INET || addr->sa_family == AF_INET6;
}

static size_t get_addr_size(struct sockaddr const *addr)
{
        assert(is_inet_family(addr));

        return addr->sa_family == AF_INET
                ? sizeof(struct in_addr)
                : sizeof(struct in6_addr);
}

static uint16_t get_addr_family(struct sockaddr const *addr)
{
        sa_family_t const family = (addr == NULL ? 0 : addr->sa_family);

        return family;
}

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
        assert(is_inet_family(addr));

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

static bool is_pm_ready(struct mptcpd_pm const *pm, char const *fname)
{
        bool const ready = mptcpd_pm_ready(pm);

        if (!ready)
                l_warn("%s: \"" MPTCP_GENL_NAME "\" family is not "
                       "yet available",
                        fname);

        return ready;
}

// ---------------------------------------------------------------------

bool mptcpd_pm_ready(struct mptcpd_pm const *pm)
{
        return pm->family != NULL;
}

bool mptcpd_pm_send_addr(struct mptcpd_pm *pm,
                         mptcpd_token_t token,
                         mptcpd_aid_t address_id,
                         struct sockaddr const *addr)
{
        if (pm == NULL || addr == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

        /*
          Payload:
              Token
              Local address ID
              Local address family
              Local address
              Local port (optional)
         */

        // Types chosen to match MPTCP genl API.
        uint16_t const family = get_addr_family(addr);
        uint16_t const port   = get_port_number(addr);

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(address_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(family))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(addr))
                + (port == 0 ? 0 : NLA_HDRLEN + NLA_ALIGN(sizeof(port)));

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
                        sizeof(address_id),
                        &address_id)
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

                return appended;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                != 0;
}

bool mptcpd_pm_remove_addr(struct mptcpd_pm *pm,
                           mptcpd_token_t token,
                           mptcpd_aid_t address_id)
{
        if (pm == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

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
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(address_id));

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

                return appended;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                != 0;
}

bool mptcpd_pm_add_subflow(struct mptcpd_pm *pm,
                           mptcpd_token_t token,
                           mptcpd_aid_t local_address_id,
                           mptcpd_aid_t remote_address_id,
                           struct sockaddr const *local_addr,
                           struct sockaddr const *remote_addr,
                           bool backup)
{
        if (pm == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

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
        uint16_t const family      = get_addr_family(remote_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_ALIGN(sizeof(family))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_address_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_address_id))
                + (local_addr == NULL
                   ? 0
                   : (NLA_HDRLEN + NLA_ALIGN(get_addr_size(local_addr))
                      + (local_port == 0
                         ? 0
                         : (NLA_HDRLEN
                            + NLA_ALIGN(sizeof(local_port))))))
                + (remote_addr == NULL
                   ? 0
                   : NLA_HDRLEN + NLA_ALIGN(get_addr_size(remote_addr))
                   + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_port)))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(backup));

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

                return appended;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                != 0;
}

bool mptcpd_pm_set_backup(struct mptcpd_pm *pm,
                          mptcpd_token_t token,
                          struct sockaddr const *local_addr,
                          struct sockaddr const *remote_addr,
                          bool backup)
{
        if (pm == NULL || local_addr == NULL || remote_addr == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

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

        uint16_t const family      = get_addr_family(local_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(family))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(local_addr))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_port))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(remote_addr))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_port))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(backup));

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

                return appended;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                != 0;
}

bool mptcpd_pm_remove_subflow(struct mptcpd_pm *pm,
                              mptcpd_token_t token,
                              struct sockaddr const *local_addr,
                              struct sockaddr const *remote_addr)
{
        if (pm == NULL || local_addr == NULL || remote_addr == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

        /*
          Payload:
              Token
              Address family
              Local address
              Local port
              Remote address
              Remote port
         */

        uint16_t const family      = get_addr_family(local_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(family))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(local_addr))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_port))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(remote_addr))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_port));

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

                return appended;
        }

        return l_genl_family_send(pm->family,
                                  msg,
                                  family_send_callback,
                                  NULL, /* user data */
                                  NULL  /* destroy */)
                != 0;
}

struct mptcpd_nm const * mptcpd_pm_get_nm(struct mptcpd_pm const *pm)
{
        return pm->nm;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
