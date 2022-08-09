# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2021-2022, Intel Corporation

#serial 2

# MPTCPD_CHECK_KERNEL
#
# Check if a MPTCP capable kernel is in use.
AC_DEFUN([MPTCPD_CHECK_KERNEL],
[
 # upstream:          /proc/sys/net/mptcp/enabled
 # multipath-tcp.org: /proc/sys/net/mptcp/mptcp_enabled
 AS_IF([test "x$with_kernel" = "xauto"],
       [mptcp_sysctl_base="/proc/sys/net/mptcp/"
        AC_CHECK_FILE(
          [${mptcp_sysctl_base}enabled],
          [with_kernel=upstream],
          [AC_CHECK_FILE(
             [${mptcp_sysctl_base}mptcp_enabled],
             [with_kernel=multipath-tcp.org],
             [with_kernel=upstream
              AC_MSG_WARN(
                [No MPTCP capable kernel detected. Assuming upstream.])
             ])
          ])
       ])
])

# MPTCPD_CHECK_KERNEL_HEADER_UPSTREAM
#
# Check if the <linux/mptcp.h> header exists, determine if it
# corresponds the upstream or multipath-tcp.org kernel, and check if
# it may be used by mptcpd since older versions may be missing
# required preprocessor symbols or enumerators.
AC_DEFUN([MPTCPD_CHECK_KERNEL_HEADER_UPSTREAM],
[
 AH_TEMPLATE([HAVE_LINUX_MPTCP_H_UPSTREAM],
             [Define to 1 if you have the upstream kernel
             <linux/mptcp.h> header.])

 AC_CACHE_CHECK([for MPTCP_ATTR_SERVER_SIDE in linux/mptcp.h],
   [mptcpd_cv_header_upstream],
   [
    # Perform a compile-time test since MPTCP_ATTR_SERVER_SIDE is an
    # enumerator, not a preprocessor symbol.
    AC_COMPILE_IFELSE([
      AC_LANG_SOURCE([
#include <linux/mptcp.h>

int get_mptcp_attr(void) { return (int) MPTCP_ATTR_SERVER_SIDE; }
      ])
    ],
    [mptcpd_cv_header_upstream=yes],
    [mptcpd_cv_header_upstream=no])
   ])

 AS_IF([test "x$mptcpd_cv_header_upstream" = "xyes"],
       [AC_DEFINE([HAVE_LINUX_MPTCP_H_UPSTREAM], [1])],
       [AC_MSG_WARN([No usable upstream <linux/mptcp.h>. Using fallback.])])
])

# MPTCPD_CHECK_KERNEL_HEADER_MPTCP_ORG
#
# Check the multipath-tcp.org kernel-specific
# /proc/sys/net/mptcp/mptcp_enabled file exists.  The
# multipath-tcp.org kernel is being used if it does.
AC_DEFUN([MPTCPD_CHECK_KERNEL_HEADER_MPTCP_ORG],
[
 AH_TEMPLATE([HAVE_LINUX_MPTCP_H_MPTCP_ORG],
             [Define to 1 if you have the multipath-tcp.org kernel
             <linux/mptcp.h> header.])

 AX_CHECK_DEFINE(
   [linux/mptcp.h],
   [MPTCP_GENL_NAME],
   [AC_DEFINE([HAVE_LINUX_MPTCP_H_MPTCP_ORG], [1])],
   [AC_MSG_WARN([No usable multipath-tcp.org <linux/mptcp.h>. Using fallback.])])
])

# MPTCPD_CHECK_LINUX_MPTCP_H
#
# Check if the <linux/mptcp.h> header is available in the system or
# user-specified include path, taking into account the detected or
# user-selected MPTCP capable kernel.
AC_DEFUN([MPTCPD_CHECK_LINUX_MPTCP_H],
[
 AC_REQUIRE([MPTCPD_CHECK_KERNEL])

 AC_CHECK_HEADERS([linux/mptcp.h],
  [
   AS_IF([test "x$with_kernel" = "xupstream"],
         [MPTCPD_CHECK_KERNEL_HEADER_UPSTREAM])

   AS_IF([test "x$with_kernel" = "xmultipath-tcp.org"],
         [MPTCPD_CHECK_KERNEL_HEADER_MPTCP_ORG])
  ],
  [
   AC_MSG_WARN([<linux/mptcp.h> header not found. Using fallback.])
  ])
])
