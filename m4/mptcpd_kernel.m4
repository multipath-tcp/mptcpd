# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2021, Intel Corporation

#serial 1

# MPTCPD_IF_UPSTREAM_KERNEL([ACTION-IF-TRUE], [ACTION-IF-FALSE])
#
# Check if the <linux/mptcp.h> header for the upstream kernel is
# available in the system include path.
AC_DEFUN([MPTCPD_IF_UPSTREAM_KERNEL],
[
 AC_CACHE_CHECK([for MPTCP_PM_CMD_ANNOUNCE in linux/mptcp.h],
   [mptcpd_cv_header_upstream],
   [
    AC_COMPILE_IFELSE([
      AC_LANG_SOURCE([
#include <linux/mptcp.h>

int announce_cmd(void) { return MPTCP_PM_CMD_ANNOUNCE; }
      ])
    ],
    [mptcpd_cv_header_upstream=yes],
    [mptcpd_cv_header_upstream=no])
   ])

 AS_IF([test "x$mptcpd_cv_header_upstream" = xyes], [$1], [$2])
])

# MPTCPD_IF_MPTCP_ORG_KERNEL([ACTION-IF-TRUE], [ACTION-IF-FALSE])
#
# Check if the <linux/mptcp.h> header for the multipath-tcp.org kernel
# is available in the system include path.
AC_DEFUN([MPTCPD_IF_MPTCP_ORG_KERNEL],
[
 AX_CHECK_DEFINE([linux/mptcp.h], [MPTCP_GENL_NAME], [$1], [$2])
])
