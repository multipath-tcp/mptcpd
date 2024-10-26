// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file call-count.c
 *
 * @brief mptcpd test plugin call count functions.
 *
 * Copyright (c) 2019, 2022, Intel Corporation
 */

#include <stdlib.h>

#include "test-plugin.h"


void call_count_reset(struct plugin_call_count *p)
{
        p->new_connection         = 0;
        p->connection_established = 0;
        p->connection_closed      = 0;
        p->new_address            = 0;
        p->address_removed        = 0;
        p->new_subflow            = 0;
        p->subflow_closed         = 0;
        p->subflow_priority       = 0;
        p->listener_created       = 0;
        p->listener_closed        = 0;
        p->new_interface          = 0;
        p->update_interface       = 0;
        p->delete_interface       = 0;
        p->new_local_address      = 0;
        p->delete_local_address   = 0;
}

bool call_count_all_positive(struct plugin_call_count const *p)
{
        return     p->new_connection         >= 0
                && p->connection_established >= 0
                && p->connection_closed      >= 0
                && p->new_address            >= 0
                && p->address_removed        >= 0
                && p->new_subflow            >= 0
                && p->subflow_closed         >= 0
                && p->subflow_priority       >= 0
                && p->listener_created       >= 0
                && p->listener_closed        >= 0
                && p->new_interface          >= 0
                && p->update_interface       >= 0
                && p->delete_interface       >= 0
                && p->new_local_address      >= 0
                && p->delete_local_address   >= 0;
}

bool call_count_is_sane(struct plugin_call_count const *p)
{
        return  // non-negative counts
                call_count_all_positive(p)

                /*
                  Some callbacks should not be called more than
                  others.
                */
                && p->connection_established <= p->new_connection
                && p->connection_closed <= p->new_connection
                && p->subflow_closed    <= p->new_subflow;
}

bool call_count_is_equal(struct plugin_call_count const *lhs,
                         struct plugin_call_count const *rhs)
{
        return lhs->new_connection         == rhs->new_connection
            && lhs->connection_established == rhs->connection_established
            && lhs->connection_closed      == rhs->connection_closed
            && lhs->new_address            == rhs->new_address
            && lhs->address_removed        == rhs->address_removed
            && lhs->new_subflow            == rhs->new_subflow
            && lhs->subflow_closed         == rhs->subflow_closed
            && lhs->subflow_priority       == rhs->subflow_priority
            && lhs->listener_created       == rhs->listener_created
            && lhs->listener_closed        == rhs->listener_closed
            && lhs->new_interface          == rhs->new_interface
            && lhs->update_interface       == rhs->update_interface
            && lhs->delete_interface       == rhs->delete_interface
            && lhs->new_local_address      == rhs->new_local_address
            && lhs->delete_local_address   == rhs->delete_local_address;
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
