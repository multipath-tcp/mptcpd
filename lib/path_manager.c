// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file lib/path_manager.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */
#include <assert.h>

#include <netinet/in.h>

#include <linux/netlink.h>
#include <linux/mptcp_genl.h>

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
                l_error("Error during genl message send.");
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
                ipv4_type = MPTCP_GENL_ATTR_LOCAL_IPV4_ADDR;
                ipv6_type = MPTCP_GENL_ATTR_LOCAL_IPV6_ADDR;
        } else {
                ipv4_type = MPTCP_GENL_ATTR_REMOTE_IPV4_ADDR;
                ipv6_type = MPTCP_GENL_ATTR_REMOTE_IPV6_ADDR;
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

// ---------------------------------------------------------------------

bool mptcpd_pm_send_addr(struct mptcpd_pm *pm,
                         mptcpd_cid_t connection_id,
                         mptcpd_aid_t address_id,
                         struct mptcpd_addr const *addr)
{
        if (pm == NULL || addr == NULL)
                return false;

        /*
          Payload:
              Connection ID
              Address ID
              Local address
              Local port (optional)
         */

        /**
         * @todo Verify that this payload size calculation is
         *       correct.  The alignment, in particular, doesn't look
         *       quite right.
         */
        size_t const payload_size =
                NLA_HDRLEN + NLA_ALIGN(sizeof(connection_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(address_id))
                + NLA_HDRLEN + NLA_ALIGN(get_addr_size(&addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(addr->port));

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_GENL_CMD_SEND_ADDR,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_GENL_ATTR_CONNECTION_ID,
                                       sizeof(connection_id),
                                       &connection_id)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_GENL_ATTR_LOCAL_ADDRESS_ID,
                        sizeof(address_id),
                        &address_id)
                && append_local_addr_attr(msg, &addr->address)
                && (addr->port == 0 ||
                    l_genl_msg_append_attr(msg,
                                           MPTCP_GENL_ATTR_LOCAL_PORT,
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
                           mptcpd_cid_t connection_id,
                           mptcpd_aid_t local_address_id,
                           mptcpd_aid_t remote_address_id,
                           struct mptcpd_addr const *local_addr,
                           struct mptcpd_addr const *remote_addr,
                           bool backup)
{
        if (pm == NULL || local_addr == NULL)
                return false;

        /*
          Payload:
              Connection ID
              Local address ID
              Local address
              Local port
              Remote address ID
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
                NLA_HDRLEN + NLA_ALIGN(sizeof(connection_id))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_address_id))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&local_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->port))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_address_id))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&remote_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_addr->port))
                + (backup ? NLA_HDRLEN : 0);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_GENL_CMD_ADD_SUBFLOW,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_GENL_ATTR_CONNECTION_ID,
                                       sizeof(connection_id),
                                       &connection_id)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_GENL_ATTR_LOCAL_ADDRESS_ID,
                        sizeof(local_address_id),
                        &local_address_id)
                && append_local_addr_attr(msg, &local_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_LOCAL_PORT,
                                          sizeof(local_addr->port),
                                          &local_addr->port)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_GENL_ATTR_REMOTE_ADDRESS_ID,
                        sizeof(remote_address_id),
                        &remote_address_id)
                && append_remote_addr_attr(msg, &remote_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_REMOTE_PORT,
                                          sizeof(remote_addr->port),
                                          &remote_addr->port)
                && (!backup
                    || l_genl_msg_append_attr(msg,
                                              MPTCP_GENL_ATTR_BACKUP,
                                              0,
                                              NULL));  // NLA_FLAG

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
                          mptcpd_cid_t connection_id,
                          struct mptcpd_addr const *local_addr,
                          struct mptcpd_addr const *remote_addr,
                          bool backup)
{
        if (pm == NULL)
                return false;

        /*
          Payload:
              Connection ID
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
                NLA_HDRLEN + NLA_ALIGN(sizeof(connection_id))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&local_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->port))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&remote_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_addr->port))
                + (backup ? NLA_HDRLEN : 0);

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_GENL_CMD_SET_BACKUP,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_GENL_ATTR_CONNECTION_ID,
                                       sizeof(connection_id),
                                       &connection_id)
                && append_local_addr_attr(msg, &local_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_LOCAL_PORT,
                                          sizeof(local_addr->port),
                                          &local_addr->port)
                && append_remote_addr_attr(msg, &remote_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_REMOTE_PORT,
                                          sizeof(remote_addr->port),
                                          &remote_addr->port)
                && (!backup
                    || l_genl_msg_append_attr(msg,
                                              MPTCP_GENL_ATTR_BACKUP,
                                              0,
                                              NULL));  // NLA_FLAG

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
                              mptcpd_cid_t connection_id,
                              struct mptcpd_addr const *local_addr,
                              struct mptcpd_addr const *remote_addr)
{
        if (pm == NULL)
                return false;

        /*
          Payload:
              Connection ID
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
                NLA_HDRLEN + NLA_ALIGN(sizeof(connection_id))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&local_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(local_addr->port))
                + NLA_HDRLEN + NLA_ALIGN(
                        get_addr_size(&remote_addr->address))
                + NLA_HDRLEN + NLA_ALIGN(sizeof(remote_addr->port));

        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(MPTCP_GENL_CMD_REMOVE_SUBFLOW,
                                     payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_GENL_ATTR_CONNECTION_ID,
                                       sizeof(connection_id),
                                       &connection_id)
                && append_local_addr_attr(msg, &local_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_LOCAL_PORT,
                                          sizeof(local_addr->port),
                                          &local_addr->port)
                && append_remote_addr_attr(msg, &remote_addr->address)
                && l_genl_msg_append_attr(msg,
                                          MPTCP_GENL_ATTR_REMOTE_PORT,
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
