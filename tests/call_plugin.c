// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file call_plugin.c
 *
 * @brief mptcpd test plugin call functions.
 *
 * Copyright (c) 2019, Intel Corporation
 */

#include <assert.h>
#include <stdlib.h>

#include <mptcpd/plugin.h>

#include "test-plugin.h"


void call_plugin_ops(struct plugin_call_count const *count,
                     char const *name,
                     mptcpd_token_t token,
                     mptcpd_aid_t raddr_id,
                     struct mptcpd_addr const *laddr,
                     struct mptcpd_addr const *raddr,
                     bool backup)
{
        assert(count != NULL);

        for (int i = 0; i < count->new_connection; ++i)
                mptcpd_plugin_new_connection(name,
                                             token,
                                             laddr,
                                             raddr,
                                             NULL);

        for (int i = 0; i < count->connection_established; ++i)
                mptcpd_plugin_connection_established(token,
                                                     laddr,
                                                     raddr,
                                                     NULL);

        for (int i = 0; i < count->new_address; ++i)
                mptcpd_plugin_new_address(token,
                                          raddr_id,
                                          raddr,
                                          NULL);

        for (int i = 0; i < count->address_removed; ++i)
                mptcpd_plugin_address_removed(token,
                                              raddr_id,
                                              NULL);

        for (int i = 0; i < count->new_subflow; ++i)
                mptcpd_plugin_new_subflow(token,
                                          laddr,
                                          raddr,
                                          backup,
                                          NULL);

        for (int i = 0; i < count->subflow_closed; ++i)
                mptcpd_plugin_subflow_closed(token,
                                             laddr,
                                             raddr,
                                             backup,
                                             NULL);

        for (int i = 0; i < count->subflow_priority; ++i)
                mptcpd_plugin_subflow_priority(token,
                                               laddr,
                                               raddr,
                                               backup,
                                               NULL);

        for (int i = 0; i < count->connection_closed; ++i)
                mptcpd_plugin_connection_closed(token, NULL);
}
