// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file addr_adv.c
 *
 * @brief MPTCP address advertiser path manager plugin.
 *
 * Copyright (c) 2020-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <errno.h>

#include <ell/util.h>  // For L_STRINGIFY needed by ELL log macros.
#include <ell/log.h>


#include <mptcpd/private/path_manager.h>
#include <mptcpd/private/configuration.h>
#include <mptcpd/id_manager.h>
#include <mptcpd/network_monitor.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>


// Allow at least this number of additional subflows for each connection
#define MPTCP_MIN_SUBFLOWS		2

// Maximum number of subflows allowed by the kernel
#define MPTCP_MAX_SUBFLOWS		8

static struct mptcpd_limit _limits[] = {
        {
        .type  = MPTCPD_LIMIT_SUBFLOWS,
        .limit = 0,
        },
        {
        .type  = MPTCPD_LIMIT_RCV_ADD_ADDRS,
        .limit = 0,
        }
};

static void update_limits(struct mptcpd_pm *pm, int delta)
{
        int subflows;

        _limits[0].limit += delta;
        subflows = _limits[0].limit;
        if (subflows < MPTCP_MIN_SUBFLOWS || subflows > MPTCP_MAX_SUBFLOWS)
                return;

        /* if the pm creates outgoing subflows, we assume this is
         * the client side, and accepts add_addrs from the server
         */
        if (pm->config->addr_flags & MPTCPD_ADDR_FLAG_SUBFLOW)
                _limits[1].limit = _limits[0].limit;

        int const result = mptcpd_pm_set_limits(pm,
                                                _limits,
                                                L_ARRAY_SIZE(_limits));

        if (result != 0 && result != ENOTSUP)
                l_warn("can't update limit to %d: %d", subflows, result);
}

static void addr_adv_new_local_address(struct mptcpd_interface const *i,
                                       struct sockaddr const *sa,
                                       struct mptcpd_pm *pm)
{
        struct mptcpd_idm *const idm = mptcpd_pm_get_idm(pm);
        mptcpd_aid_t const id = mptcpd_idm_get_id(idm, sa);

        if (id == 0) {
                l_error("Unable to map addr to ID.");
                return;
        }

        uint32_t       const flags = pm->config->addr_flags;
        mptcpd_token_t const token = 0;

        update_limits(pm, 1);

        if (mptcpd_pm_add_addr(pm, sa, id, flags, i->index, token) != 0)
                l_error("Unable to advertise IP address.");
}

static void addr_adv_delete_local_address(
        struct mptcpd_interface const *i,
        struct sockaddr const *sa,
        struct mptcpd_pm *pm)
{
        (void) i;

        struct mptcpd_idm *const idm = mptcpd_pm_get_idm(pm);
        mptcpd_aid_t const id = mptcpd_idm_remove_id(idm, sa);

        if (id == 0) {
                // Not necessarily an error.
                l_info("No address ID associated with addr.");
                return;
        }

        mptcpd_token_t const token = 0;

        update_limits(pm, -1);

        if (mptcpd_pm_remove_addr(pm, id, token) != 0)
                l_error("Unable to stop advertising IP address.");
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_local_address    = addr_adv_new_local_address,
        .delete_local_address = addr_adv_delete_local_address
};

static int addr_adv_init(struct mptcpd_pm *pm)
{
        static char const name[] = "addr_adv";

        update_limits(pm, MPTCP_MIN_SUBFLOWS);

        if (!mptcpd_plugin_register_ops(name, &pm_ops)) {
                l_error("Failed to initialize address advertiser "
                        "path manager plugin.");

                return -1;
        }

        l_info("MPTCP address advertiser path manager initialized.");

        return 0;
}

static void addr_adv_exit(struct mptcpd_pm *pm)
{
        (void) pm;

        l_info("MPTCP address advertiser path manager exited.");
}

MPTCPD_PLUGIN_DEFINE(addr_adv,
                     "Address advertiser path manager",
                     MPTCPD_PLUGIN_PRIORITY_DEFAULT,
                     addr_adv_init,
                     addr_adv_exit)


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
