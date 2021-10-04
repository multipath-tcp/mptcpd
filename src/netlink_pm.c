// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file netlink_pm.c
 *
 * @brief Kernel generic netlink path manager details.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <errno.h>

#include <sys/socket.h>
#include <netinet/in.h>

#include <ell/genl.h>
#include <ell/log.h>
#include <ell/util.h>

// For command attibutes.  Same API applies to multipath-tcp.org
// kernel.
#include <mptcpd/private/mptcp_upstream.h>

#include <mptcpd/private/path_manager.h>
#include <mptcpd/types.h>

#include "commands.h"
#include "netlink_pm.h"

/// Directory containing MPTCP sysctl variable entries.
#define MPTCP_SYSCTL_BASE "/proc/sys/net/mptcp/"

/// Constuct full path for MPTCP sysctl variable "@a name".
#define MPTCP_SYSCTL_VARIABLE(name) MPTCP_SYSCTL_BASE #name

/// Get multipath-tcp.org kernel generic netlink PM characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm_mptcp_org(void);

/// Get upstream kernel generic netlink PM characteristics.
struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm_upstream(void);

/**
 * @brief Verify that MPTCP is enabled at run-time in the kernel.
 *
 * Check that MPTCP is enabled through the specified @c sysctl
 * variable.
 */
static bool check_kernel_mptcp_enabled(char const *path,
                                       char const *variable,
                                       int enable_val)
{
        FILE *const f = fopen(path, "r");

        if (f == NULL)
                return false;  // Not using kernel that supports given
                               // MPTCP sysctl variable.

        int enabled = 0;
        int const n = fscanf(f, "%d", &enabled);

        fclose(f);

        if (n == 1) {
                if (enabled == 0) {
                        l_error("MPTCP is not enabled in the kernel.");
                        l_error("Try 'sysctl -w net.mptcp.%s=%d'.",
                                variable,
                                enable_val);

                        /*
                          Mptcpd should not set this variable since it
                          would need to be run as root in order to
                          gain write access to files in
                          /proc/sys/net/mptcp/ owned by root.
                          Ideally, mptcpd should not be run as the
                          root user.
                        */
                }
        } else {
                l_error("Unable to determine if MPTCP is enabled.");
        }

        return enabled;
}

/// Are we being run under a MPTCP-capable upstream kernel?
static bool is_upstream_kernel(void)
{
        static char const path[] = MPTCP_SYSCTL_VARIABLE(enabled);
        static char const name[] = "enabled";
        static int  const enable_val = 1;

        return check_kernel_mptcp_enabled(path, name, enable_val);
}

/// Are we being run under a MPTCP-capable multipath-tcp.org kernel?
static bool is_mptcp_org_kernel(void)
{
        static char const path[] = MPTCP_SYSCTL_VARIABLE(mptcp_enabled);
        static char const name[] = "mptcp_enabled";
        static int  const enable_val = 2;  // or 1

        return check_kernel_mptcp_enabled(path, name, enable_val);
}

struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm(void)
{
        if (is_upstream_kernel()) {
                return mptcpd_get_netlink_pm_upstream();
        } else if (is_mptcp_org_kernel()) {
                return mptcpd_get_netlink_pm_mptcp_org();
        } else {
                return NULL;
        }
}

// ---------------------------------------------------------------

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

// ---------------------------------------------------------------

int netlink_pm_add_addr(uint8_t cmd,
                        struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t id,
                        mptcpd_token_t token)
{
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
                + MPTCPD_NLA_ALIGN_ADDR(addr)
                + MPTCPD_NLA_ALIGN_OPT(port);

        // cmd == MPTCP_CMD_ANNOUNCE
        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd, payload_size);

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
                                  NULL  /* destroy */);
}

int netlink_pm_remove_addr(uint8_t cmd,
                           struct mptcpd_pm *pm,
                           mptcpd_aid_t id,
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
                + MPTCPD_NLA_ALIGN(id);

        // cmd == MPTCP_CMD_REMOVE
        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd, payload_size);

        bool const appended =
                l_genl_msg_append_attr(msg,
                                       MPTCP_ATTR_TOKEN,
                                       sizeof(token),
                                       &token)
                && l_genl_msg_append_attr(
                        msg,
                        MPTCP_ATTR_LOC_ID,
                        sizeof(id),
                        &id);

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

int netlink_pm_add_subflow(uint8_t cmd,
                           struct mptcpd_pm *pm,
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
              Remote address
              Remote port

              Optional attributes:
                  Local address
                  Local port
                  Backup priority flag
                  Network interface index
         */

        uint16_t const family      = mptcpd_get_addr_family(remote_addr);
        uint16_t const local_port  = get_port_number(local_addr);
        uint16_t const remote_port = get_port_number(remote_addr);
        // uint16_t per MPTCP genl API.

        if (remote_port == 0)
                return EINVAL;

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
                   : (MPTCPD_NLA_ALIGN_ADDR(local_addr)
                      + (MPTCPD_NLA_ALIGN_OPT(local_port))))
                + MPTCPD_NLA_ALIGN_ADDR(remote_addr)
                + MPTCPD_NLA_ALIGN(remote_port)
                + MPTCPD_NLA_ALIGN(backup);

        // cmd == MPTCP_CMD_SUB_CREATE
        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd, payload_size);

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
                && l_genl_msg_append_attr(msg,
                                          MPTCP_ATTR_FAMILY,
                                          sizeof(family),
                                          &family)
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
                                  "add_subflow", /* user data */
                                  NULL  /* destroy */)
                == 0;
}

int netlink_pm_remove_subflow(uint8_t cmd,
                              struct mptcpd_pm *pm,
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
                + MPTCPD_NLA_ALIGN_ADDR(local_addr)
                + MPTCPD_NLA_ALIGN(local_port)
                + MPTCPD_NLA_ALIGN_ADDR(remote_addr)
                + MPTCPD_NLA_ALIGN(remote_port);

        // cmd == MPTCP_CMD_SUB_DESTROY
        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd, payload_size);

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

int netlink_pm_set_backup(uint8_t cmd,
                          struct mptcpd_pm *pm,
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
                + MPTCPD_NLA_ALIGN_ADDR(local_addr)
                + MPTCPD_NLA_ALIGN(local_port)
                + MPTCPD_NLA_ALIGN_ADDR(remote_addr)
                + MPTCPD_NLA_ALIGN(remote_port)
                + MPTCPD_NLA_ALIGN(backup);

        // cmd == MPTCP_CMD_SUB_PRIORITY
        struct l_genl_msg *const msg =
                l_genl_msg_new_sized(cmd,
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


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
