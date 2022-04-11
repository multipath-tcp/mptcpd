// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file call_plugin.c
 *
 * @brief mptcpd test plugin call functions.
 *
 * Copyright (c) 2019-2022, Intel Corporation
 */

#include <stdlib.h>

#include <mptcpd/private/plugin.h>

#include "test-plugin.h"

#undef NDEBUG
#include <assert.h>


void call_plugin_ops(struct plugin_call_count const *count,
                     struct plugin_call_args const *args)
{
        assert(count != NULL);
        assert(args != NULL);

        for (int i = 0; i < count->new_connection; ++i)
                mptcpd_plugin_new_connection(args->name,
                                             args->token,
                                             args->laddr,
                                             args->raddr,
                                             args->pm);

        for (int i = 0; i < count->connection_established; ++i)
                mptcpd_plugin_connection_established(args->token,
                                                     args->laddr,
                                                     args->raddr,
                                                     args->pm);

        for (int i = 0; i < count->new_address; ++i)
                mptcpd_plugin_new_address(args->token,
                                          args->raddr_id,
                                          args->raddr,
                                          args->pm);

        for (int i = 0; i < count->address_removed; ++i)
                mptcpd_plugin_address_removed(args->token,
                                              args->raddr_id,
                                              args->pm);

        for (int i = 0; i < count->new_subflow; ++i)
                mptcpd_plugin_new_subflow(args->token,
                                          args->laddr,
                                          args->raddr,
                                          args->backup,
                                          args->pm);

        for (int i = 0; i < count->subflow_closed; ++i)
                mptcpd_plugin_subflow_closed(args->token,
                                             args->laddr,
                                             args->raddr,
                                             args->backup,
                                             args->pm);

        for (int i = 0; i < count->subflow_priority; ++i)
                mptcpd_plugin_subflow_priority(args->token,
                                               args->laddr,
                                               args->raddr,
                                               args->backup,
                                               args->pm);

        for (int i = 0; i < count->connection_closed; ++i)
                mptcpd_plugin_connection_closed(args->token, args->pm);

        for (int i = 0; i < count->new_interface; ++i)
                mptcpd_plugin_new_interface(args->interface, args->pm);

        for (int i = 0; i < count->update_interface; ++i)
                mptcpd_plugin_update_interface(args->interface, args->pm);

        for (int i = 0; i < count->delete_interface; ++i)
                mptcpd_plugin_delete_interface(args->interface, args->pm);

        for (int i = 0; i < count->new_local_address; ++i)
                mptcpd_plugin_new_local_address(args->interface,
                                                args->laddr,
                                                args->pm);

        for (int i = 0; i < count->delete_local_address; ++i)
                mptcpd_plugin_delete_local_address(args->interface,
                                                   args->laddr,
                                                   args->pm);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
