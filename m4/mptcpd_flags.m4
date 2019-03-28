# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2017-2019, Intel Corporation

#serial 1

# MPTCPD_ADD_COMPILE_FLAG(FLAG, [ACTION-FAILURE])
#
# Check if the compiler accepts the given FLAG.  If it does append it
# to CFLAGS.  Otherwise execute the given failure action.
AC_DEFUN([MPTCPD_ADD_COMPILE_FLAG],
         [AX_CHECK_COMPILE_FLAG([$1],
	                        [AX_APPEND_FLAG([$1], [CFLAGS])], [$2])
         ])

# MPTCPD_ADD_LINK_FLAG(FLAG, [ACTION-FAILURE])
#
# Check if the linker accepts the given FLAG.  If it does append it
# to LDFLAGS.  Otherwise execute the given failure action.
AC_DEFUN([MPTCPD_ADD_LINK_FLAG],
         [AX_CHECK_LINK_FLAG([$1],
	                     [AX_APPEND_FLAG([$1], [LDFLAGS])], [$2])
         ])
