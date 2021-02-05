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

#include <assert.h>
#include <errno.h>

#include <netinet/in.h>

#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error().
#include <ell/log.h>

#include <mptcpd/path_manager.h>
#include <mptcpd/private/path_manager.h>
#include <mptcpd/plugin.h>
#include <mptcpd/mptcp_private.h>


static bool is_pm_ready(struct mptcpd_pm const *pm, char const *fname)
{
        bool const ready = mptcpd_pm_ready(pm);

        if (!ready)
                l_warn("%s: MPTCP family is not yet available",
                        fname);

        return ready;
}

// --------------------------------------------------------------------

bool mptcpd_pm_ready(struct mptcpd_pm const *pm)
{
        return pm->family != NULL;
}

int mptcpd_pm_add_addr(struct mptcpd_pm *pm,
                       struct sockaddr const *addr,
                       mptcpd_aid_t address_id,
                       uint32_t flags,
                       int index,
                       mptcpd_token_t token)
{
        if (pm == NULL || addr == NULL || address_id == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->add_addr == NULL)
                return ENOTSUP;

        return ops->add_addr(pm,
                             addr,
                             address_id,
                             flags,
                             index,
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

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->remove_addr == NULL)
                return ENOTSUP;

        return ops->remove_addr(pm, address_id, token);
}

int mptcpd_pm_get_addr(struct mptcpd_pm *pm,
                       mptcpd_aid_t id,
                       mptcpd_pm_get_addr_cb callback,
                       void *data)
{
        if (pm == NULL || id == 0 || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->get_addr == NULL)
                return ENOTSUP;

        return ops->get_addr(pm, id, callback, data);
}

int mptcpd_pm_dump_addrs(struct mptcpd_pm *pm,
                         mptcpd_pm_get_addr_cb callback,
                         void *data)
{
        if (pm == NULL || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->dump_addrs == NULL)
                return ENOTSUP;

        return ops->dump_addrs(pm, callback, data);
}

int mptcpd_pm_flush_addrs(struct mptcpd_pm *pm)
{
        if (pm == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->flush_addrs == NULL)
                return ENOTSUP;

        return ops->flush_addrs(pm);
}

int mptcpd_pm_set_limits(struct mptcpd_pm *pm,
                         struct mptcpd_limit const *limits,
                         size_t len)
{
        if (pm == NULL || limits == NULL || len == 0)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->set_limits == NULL)
                return ENOTSUP;

        return ops->set_limits(pm, limits, len);
}

int mptcpd_pm_get_limits(struct mptcpd_pm *pm,
                         mptcpd_pm_get_limits_cb callback,
                         void *data)
{
        if (pm == NULL || callback == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->get_limits == NULL)
                return ENOTSUP;

        return ops->get_limits(pm, callback, data);
}

int mptcpd_pm_add_subflow(struct mptcpd_pm *pm,
                          mptcpd_token_t token,
                          mptcpd_aid_t local_address_id,
                          mptcpd_aid_t remote_address_id,
                          struct sockaddr const *local_addr,
                          struct sockaddr const *remote_addr,
                          bool backup)
{
        if (pm == NULL)
                return EINVAL;

        if (!is_pm_ready(pm, __func__))
                return EAGAIN;

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->add_subflow == NULL)
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

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->set_backup == NULL)
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

        struct mptcpd_pm_cmd_ops const *const ops = pm->cmd_ops;

        if (ops->remove_subflow == NULL)
                return ENOTSUP;

        return ops->remove_subflow(pm,
                                   token,
                                   local_addr,
                                   remote_addr);
}

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
