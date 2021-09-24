// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file lib/path_manager.c
 *
 * @brief mptcpd generic netlink commands.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <assert.h>
#include <errno.h>

#include <netinet/in.h>

#include <ell/genl.h>
#include <ell/queue.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>

#include <mptcpd/path_manager.h>
#include <mptcpd/private/path_manager.h>
#include <mptcpd/plugin.h>
#include <mptcpd/private/netlink_pm.h>


// -------------------------------------------------------------------

static bool is_pm_ready(struct mptcpd_pm const *pm, char const *fname)
{
        bool const ready = mptcpd_pm_ready(pm);

        if (!ready)
                l_warn("%s: MPTCP family is not yet available",
                        fname);

        return ready;
}

// --------------------------------------------------------------------

bool mptcpd_pm_register_ops(struct mptcpd_pm *pm,
                            struct mptcpd_pm_ops const *ops,
                            void *user_data)
{
        if (pm == NULL || ops == NULL)
                return false;

        if (ops->ready        == NULL
            && ops->not_ready == NULL) {
                l_error("No path manager event tracking "
                        "ops were set.");

                return false;
        }

        struct pm_ops_info *const info = l_malloc(sizeof(*info));
        info->ops = ops;
        info->user_data = user_data;

        bool const registered = l_queue_push_tail(pm->event_ops, info);

        if (!registered)
                l_free(info);

        return registered;
}

bool mptcpd_pm_ready(struct mptcpd_pm const *pm)
{
        return pm->family != NULL;
}

// -------------------------------------------------------------------

int mptcpd_kpm_add_addr(struct mptcpd_pm *pm,
                        struct sockaddr const *addr,
                        mptcpd_aid_t address_id,
                        mptcpd_flags_t flags,
                        int index)
{
        if (pm == NULL || addr == NULL || address_id == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->add_addr == NULL)
                return ENOTSUP;

        return ops->add_addr(pm,
                             addr,
                             address_id,
                             flags,
                             index);
}

int mptcpd_kpm_remove_addr(struct mptcpd_pm *pm, mptcpd_aid_t address_id)
{
        if (pm == NULL || address_id == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->remove_addr == NULL)
                return ENOTSUP;

        return ops->remove_addr(pm, address_id);
}

int mptcpd_kpm_get_addr(struct mptcpd_pm *pm,
                        mptcpd_aid_t id,
                        mptcpd_pm_get_addr_cb callback,
                        void *data)
{
        if (pm == NULL || id == 0 || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->get_addr == NULL)
                return ENOTSUP;

        return ops->get_addr(pm, id, callback, data);
}

int mptcpd_kpm_dump_addrs(struct mptcpd_pm *pm,
                          mptcpd_pm_get_addr_cb callback,
                          void *data)
{
        if (pm == NULL || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->dump_addrs == NULL)
                return ENOTSUP;

        return ops->dump_addrs(pm, callback, data);
}

int mptcpd_kpm_flush_addrs(struct mptcpd_pm *pm)
{
        if (pm == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->flush_addrs == NULL)
                return ENOTSUP;

        return ops->flush_addrs(pm);
}

int mptcpd_kpm_set_limits(struct mptcpd_pm *pm,
                          struct mptcpd_limit const *limits,
                          size_t len)
{
        if (pm == NULL || limits == NULL || len == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->set_limits == NULL)
                return ENOTSUP;

        return ops->set_limits(pm, limits, len);
}

int mptcpd_kpm_get_limits(struct mptcpd_pm *pm,
                          mptcpd_pm_get_limits_cb callback,
                          void *data)
{
        if (pm == NULL || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->get_limits == NULL)
                return ENOTSUP;

        return ops->get_limits(pm, callback, data);
}

int mptcpd_kpm_set_flags(struct mptcpd_pm *pm,
                         struct sockaddr const *addr,
                         mptcpd_flags_t flags)
{
        if (pm == NULL || addr == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_kpm_cmd_ops const *const ops =
                pm->netlink_pm->kcmd_ops;

        if (ops == NULL || ops->set_flags == NULL)
                return ENOTSUP;

        return ops->set_flags(pm, addr, flags);
}

// -------------------------------------------------------------------

int mptcpd_pm_add_addr(struct mptcpd_pm *pm,
                       struct sockaddr const *addr,
                       mptcpd_aid_t address_id,
                       mptcpd_token_t token)
{
        if (pm == NULL || addr == NULL || address_id == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops =
                pm->netlink_pm->cmd_ops;

        if (ops == NULL || ops->add_addr == NULL)
                return ENOTSUP;

        return ops->add_addr(pm,
                             addr,
                             address_id,
                             token);
}

int mptcpd_pm_remove_addr(struct mptcpd_pm *pm,
                          mptcpd_aid_t address_id,
                          mptcpd_token_t token)
{
        if (pm == NULL || address_id == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops =
                pm->netlink_pm->cmd_ops;

        if (ops == NULL || ops->remove_addr == NULL)
                return ENOTSUP;

        return ops->remove_addr(pm, address_id, token);
}

int mptcpd_pm_add_subflow(struct mptcpd_pm *pm,
                          mptcpd_token_t token,
                          mptcpd_aid_t local_address_id,
                          mptcpd_aid_t remote_address_id,
                          struct sockaddr const *local_addr,
                          struct sockaddr const *remote_addr,
                          bool backup)
{
        if (pm == NULL || remote_addr == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops =
                pm->netlink_pm->cmd_ops;

        if (ops == NULL || ops->add_subflow == NULL)
                return ENOTSUP;

        return ops->add_subflow(pm,
                                token,
                                local_address_id,
                                remote_address_id,
                                local_addr,
                                remote_addr,
                                backup);
}

int mptcpd_pm_set_backup(struct mptcpd_pm *pm,
                         mptcpd_token_t token,
                         struct sockaddr const *local_addr,
                         struct sockaddr const *remote_addr,
                         bool backup)
{
        if (pm == NULL || local_addr == NULL || remote_addr == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops =
                pm->netlink_pm->cmd_ops;

        if (ops == NULL || ops->set_backup == NULL)
                return ENOTSUP;

        return ops->set_backup(pm,
                               token,
                               local_addr,
                               remote_addr,
                               backup);
}

int mptcpd_pm_remove_subflow(struct mptcpd_pm *pm,
                             mptcpd_token_t token,
                             struct sockaddr const *local_addr,
                             struct sockaddr const *remote_addr)
{
        if (pm == NULL || local_addr == NULL || remote_addr == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops =
                pm->netlink_pm->cmd_ops;

        if (ops == NULL || ops->remove_subflow == NULL)
                return ENOTSUP;

        return ops->remove_subflow(pm,
                                   token,
                                   local_addr,
                                   remote_addr);
}

// -------------------------------------------------------------------

struct mptcpd_nm const * mptcpd_pm_get_nm(struct mptcpd_pm const *pm)
{
        return pm->nm;
}

struct mptcpd_idm * mptcpd_pm_get_idm(struct mptcpd_pm const *pm)
{
        return pm->idm;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
