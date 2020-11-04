// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file commands.c
 *
 * @brief mptcpd generic netlink command utilities.
 *
 * Copyright (c) 2017-2020, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/config-private.h>
#endif

#define _POSIX_C_SOURCE 200112L  ///< For XSI-compliant strerror_r().

#include <string.h>

#include <ell/genl.h>
#include <ell/util.h>  // For L_STRINGIFY needed by l_error(), etc.
#include <ell/log.h>

#include "commands.h"


void mptcpd_family_send_callback(struct l_genl_msg *msg, void *user_data)
{
        (void) user_data;

        int const error = l_genl_msg_get_error(msg);

        if (error < 0) {
                // Error during send.  Likely insufficient perms.

                char errmsg[80];
                int const r = strerror_r(-error,
                                         errmsg,
                                         L_ARRAY_SIZE(errmsg));

                l_error("Path manager command error: %s",
                        r == 0 ? errmsg : "<unknown error>");
        }
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
