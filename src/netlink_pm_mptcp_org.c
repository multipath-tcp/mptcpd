// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file netlink_pm_mptcp_org.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <stdio.h>

#include <ell/log.h>
#include <ell/util.h>

#include "netlink_pm.h"

#include <mptcpd/private/mptcp_org.h>
#include <mptcpd/private/netlink_pm.h>
#include <mptcpd/private/path_manager.h>

/// Directory containing MPTCP sysctl variable entries.
#define MPTCP_SYSCTL_BASE "/proc/sys/net/mptcp/"

/**
 * @brief Maximum path manager name length.
 *
 * @note @c MPTCP_PM_NAME_MAX is defined in the internal
 *       @c <net/mptcp.h> header in the multipath-tcp.org kernel.
 */
#ifndef MPTCP_PM_NAME_MAX
# define MPTCP_PM_NAME_MAX 16
#endif


/**
 * @brief Verify that the MPTCP "netlink" path manager is selected.
 *
 * Mptcpd requires MPTCP the "netlink" path manager to be selected at
 * run-time when using the multipath-tcp.org kernel.  Issue a warning
 * if it is not selected.
 */
static void check_kernel_mptcp_path_manager(void)
{
        static char const mptcp_path_manager[] =
                MPTCP_SYSCTL_BASE "mptcp_path_manager";

        FILE *const f = fopen(mptcp_path_manager, "r");

        if (f == NULL)
                return;  // Not using multipath-tcp.org kernel.

        char pm[MPTCP_PM_NAME_MAX] = { 0 };

        static char const MPTCP_PM_NAME_FMT[] =
                "%" L_STRINGIFY(MPTCP_PM_NAME_MAX) "s";

        int const n = fscanf(f, MPTCP_PM_NAME_FMT, pm);

        fclose(f);

        if (n == 1) {
                if (strcmp(pm, "netlink") != 0) {
                        /*
                          "netlink" could be set as the default.  It
                          may not appear as "netlink", which is why
                          "may" is used instead of "is" in the warning
                          diagnostic below
                        */
                        l_warn("MPTCP 'netlink' path manager may "
                               "not be selected in the kernel.");
                        l_warn("Try 'sysctl -w "
                               "net.mptcp.mptcp_path_manager=netlink'.");

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
                l_warn("Unable to determine selected MPTCP "
                       "path manager.");
        }
}

// ---------------------------------------------------------------------

static int mptcp_org_add_addr(struct mptcpd_pm *pm,
                              struct sockaddr const *addr,
                              mptcpd_aid_t id,
                              mptcpd_token_t token)
{
        return netlink_pm_add_addr(MPTCP_CMD_ANNOUNCE,
                                   pm,
                                   addr,
                                   id,
                                   token);
}

static int mptcp_org_remove_addr(struct mptcpd_pm *pm,
                                 mptcpd_aid_t address_id,
                                 mptcpd_token_t token)
{
        return netlink_pm_remove_addr(MPTCP_CMD_REMOVE,
                                      pm,
                                      address_id,
                                      token);
}

static int mptcp_org_add_subflow(struct mptcpd_pm *pm,
                                 mptcpd_token_t token,
                                 mptcpd_aid_t local_address_id,
                                 mptcpd_aid_t remote_address_id,
                                 struct sockaddr const *local_addr,
                                 struct sockaddr const *remote_addr,
                                 bool backup)
{
        return netlink_pm_add_subflow(MPTCP_CMD_SUB_CREATE,
                                      pm,
                                      token,
                                      local_address_id,
                                      remote_address_id,
                                      local_addr,
                                      remote_addr,
                                      backup);
}

static int mptcp_org_remove_subflow(struct mptcpd_pm *pm,
                                    mptcpd_token_t token,
                                    struct sockaddr const *local_addr,
                                    struct sockaddr const *remote_addr)
{
        return netlink_pm_remove_subflow(MPTCP_CMD_SUB_DESTROY,
                                         pm,
                                         token,
                                         local_addr,
                                         remote_addr);
}

static int mptcp_org_set_backup(struct mptcpd_pm *pm,
                                mptcpd_token_t token,
                                struct sockaddr const *local_addr,
                                struct sockaddr const *remote_addr,
                                bool backup)
{
        return netlink_pm_set_backup(MPTCP_CMD_SUB_PRIORITY,
                                     pm,
                                     token,
                                     local_addr,
                                     remote_addr,
                                     backup);
}

static struct mptcpd_pm_cmd_ops const cmd_ops =
{
        .add_addr       = mptcp_org_add_addr,
        .remove_addr    = mptcp_org_remove_addr,
        .add_subflow    = mptcp_org_add_subflow,
        .remove_subflow = mptcp_org_remove_subflow,
        .set_backup     = mptcp_org_set_backup,
};

static struct mptcpd_netlink_pm const npm = {
        .name    = MPTCP_GENL_NAME,
        .group   = MPTCP_GENL_EV_GRP_NAME,
        .cmd_ops = &cmd_ops
};

struct mptcpd_netlink_pm const *mptcpd_get_netlink_pm_mptcp_org(void)
{
        check_kernel_mptcp_path_manager();

        return &npm;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
