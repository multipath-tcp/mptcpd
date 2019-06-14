// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file lib/path_manager.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().

#include <assert.h>

#include <netinet/in.h>

#include <linux/netlink.h>
#include <linux/mptcp.h>

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

static size_t get_addr_size(struct mptcpd_in_addr const *addr)
{
        assert(addr->family == AF_INET || addr->family == AF_INET6);

        return addr->family == AF_INET
                ? sizeof(struct in_addr)
                : sizeof(struct in6_addr);
}

static bool append_addr_attr(struct l_genl_msg *msg,
                             struct mptcpd_in_addr const *addr,
                             bool local)
{
        assert(addr->family == AF_INET || addr->family == AF_INET6);

        uint16_t ipv4_type, ipv6_type;

        if (local) {
                ipv4_type = MPTCP_ATTR_SADDR4;
                ipv6_type = MPTCP_ATTR_SADDR6;
        } else {
                ipv4_type = MPTCP_ATTR_DADDR4;
                ipv6_type = MPTCP_ATTR_DADDR6;
        }

        return addr->family == AF_INET
                ? l_genl_msg_append_attr(msg,
                                         ipv4_type,
                                         sizeof(struct in_addr),
                                         &addr->addr.addr4)
                : l_genl_msg_append_attr(msg,
                                         ipv6_type,
                                         sizeof(struct in6_addr),
                                         &addr->addr.addr6);
}

static bool append_local_addr_attr(struct l_genl_msg *msg,
                                   struct mptcpd_in_addr const *addr)
{
        static bool const local = true;

        return append_addr_attr(msg, addr, local);
}

static bool append_remote_addr_attr(struct l_genl_msg *msg,
                                    struct mptcpd_in_addr const *addr)
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
                         struct mptcpd_addr const *addr)
{
        if (pm == NULL || addr == NULL)
                return false;

        if (!is_pm_ready(pm, __func__))
                return false;

        assert(sizeof(addr->address.family) == sizeof(uint16_t));

        /*
          Payload:
              Token
              Local address ID
              Local address family
              Local address
              Local port (optional)
         */

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(address_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(addr->address.family))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(&addr->address))
                + (addr->port == 0
                   ? 0
                   : NLA_HDRLEN + NLA_ALIGN(sizeof(addr->port)));

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
                        sizeof(addr->address.family),  // sizeof(uint16_t)
                        &addr->address.family)
                && append_local_addr_attr(msg, &addr->address)
                && (addr->port == 0 ||
                    l_genl_msg_append_attr(msg,
                                           MPTCP_ATTR_SPORT,
                                           sizeof(addr->port),
                                           &addr->port));

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
                           struct mptcpd_addr const *local_addr,
                           struct mptcpd_addr const *remote_addr,
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
         *
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_address_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_address_id))
                + (local_addr == NULL
                   ? 0
                   : (NLA_HDRLEN
                      + NLA_ALIGN(get_addr_size(&local_addr->address))
                      + (local_addr->port == 0
                         ? 0
                         : (NLA_HDRLEN
                            + NLA_ALIGN(sizeof(local_addr->port))))))
                + (remote_addr == NULL
                   ? 0
                   : (NLA_HDRLEN
                      + NLA_ALIGN(sizeof(remote_addr->address.family))
                      + NLA_HDRLEN + NLA_ALIGN(
                              get_addr_size(&remote_addr->address))
                      + NLA_HDRLEN
                      + NLA_ALIGN(sizeof(remote_addr->port))))
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
                    || (append_local_addr_attr(msg, &local_addr->address)
                        && (local_addr->port == 0
                            || l_genl_msg_append_attr(msg,
                                                      MPTCP_ATTR_SPORT,
                                                      sizeof(local_addr->port),
                                                      &local_addr->port))))
                && (remote_addr == NULL
                    || (l_genl_msg_append_attr(
                                msg,
                                MPTCP_ATTR_FAMILY,
                                sizeof(remote_addr->address.family),
                                &remote_addr->address.family)
                        && append_remote_addr_attr(msg, &remote_addr->address)
                        && remote_addr->port != 0
                        && l_genl_msg_append_attr(msg,
                                                  MPTCP_ATTR_DPORT,
                                                  sizeof(remote_addr->port),
                                                  &remote_addr->port)))
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
                          struct mptcpd_addr const *local_addr,
                          struct mptcpd_addr const *remote_addr,
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

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->address.family))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&local_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->port))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&remote_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_addr->port))
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
                        sizeof(local_addr->address.family),
                        &local_addr->address.family)
                && append_local_addr_attr(msg, &local_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_SPORT,
                                          sizeof(local_addr->port),
                                          &local_addr->port)
                && append_remote_addr_attr(msg, &remote_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_DPORT,
                                          sizeof(remote_addr->port),
                                          &remote_addr->port)
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
                              struct mptcpd_addr const *local_addr,
                              struct mptcpd_addr const *remote_addr)
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

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(token))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->address.family))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&local_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->port))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&remote_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_addr->port));

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
                        sizeof(local_addr->address.family),
                        &local_addr->address.family)
                && append_local_addr_attr(msg, &local_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_SPORT,
                                          sizeof(local_addr->port),
                                          &local_addr->port)
                && append_remote_addr_attr(msg, &remote_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_DPORT,
                                          sizeof(remote_addr->port),
                                          &remote_addr->port);

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
