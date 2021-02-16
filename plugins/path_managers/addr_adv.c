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

#include <ell/util.h>  // For L_STRINGIFY needed by ELL log macros.
#include <ell/log.h>

#include <mptcpd/id_manager.h>
#include <mptcpd/network_monitor.h>
#include <mptcpd/path_manager.h>
#include <mptcpd/plugin.h>


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

        uint32_t       const flags = 0;
        mptcpd_token_t const token = 0;

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

        if (mptcpd_pm_remove_addr(pm, id, token) != 0)
                l_error("Unable to stop advertising IP address.");
}

static struct mptcpd_plugin_ops const pm_ops = {
        .new_local_address    = addr_adv_new_local_address,
        .delete_local_address = addr_adv_delete_local_address
};

static int addr_adv_init(struct mptcpd_pm *pm)
{
        (void) pm;

        static char const name[] = "addr_adv";

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
