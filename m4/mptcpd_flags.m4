# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright (c) 2017-2019, Intel Corporation

#serial 2

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

# MPTCPD_ADD_ASAN_FLAG
#
# Add support for enabling GCC/Clang memory error detector
# (AddressSanitizer) instrumentation.
#
# @todo Reduce code duplication between the
#       MPTCPD_ADD_{A,L,UB}SAN_SUPPORT macros.
AC_DEFUN([MPTCPD_ADD_ASAN_SUPPORT],
  [
   AC_ARG_ENABLE([asan],
                 [AS_HELP_STRING([--enable-asan],
                                 [Enable memory error detector])],
                 [],
                 [enable_asan=no])

   AS_IF([test "x$enable_asan" = "xyes"],
     [
      MPTCPD_ADD_COMPILE_FLAG(
        [-fsanitize=address],
        [AC_MSG_ERROR([Address sanitizer not supported by the compiler])])

      dnl -fno-omit-frame-pointer to get nicer stack traces.
      MPTCPD_ADD_COMPILE_FLAG([-fno-omit-frame-pointer])

      AS_IF([test "x$enable_shared" != "xyes"],
            [MPTCPD_ADD_COMPILE_FLAG([-static-libasan])])

      dnl Explicitly check for libasan through AC_CHECK_LIB instead
      dnl of AC_SEARCH_LIBS since we're relying on the existence of
      dnl the _init symbol found in all libraries.  We also don't
      dnl want to explicitly add -lasan to the link command line
      dnl since the compiler driver will do that automatically.
      AC_CHECK_LIB(
        [asan],
        [_init],
        [],
        [AC_MSG_ERROR([libasan is needed for memory error detector])])
    ])
  ])

# MPTCPD_ADD_LSAN_FLAG
#
# Add support for enabling GCC/Clang memory leak detector
# (LeakSanitizer) instrumentation.  This is the leak-only
# functionality found in the address sanitizer.
#
# @todo Reduce code duplication between the
#       MPTCPD_ADD_{A,L,UB}SAN_SUPPORT macros.
AC_DEFUN([MPTCPD_ADD_LSAN_SUPPORT],
  [
   AC_ARG_ENABLE([lsan],
                 [AS_HELP_STRING([--enable-lsan],
                                 [Enable memory leak detector])],
                 [],
                 [enable_lsan=no])

   AS_IF([test "x$enable_lsan" = "xyes"],
     [
      MPTCPD_ADD_COMPILE_FLAG(
        [-fsanitize=leak],
        [AC_MSG_ERROR([Leak sanitizer not supported by the compiler])])

      dnl -fno-omit-frame-pointer to get nicer stack traces.
      MPTCPD_ADD_COMPILE_FLAG([-fno-omit-frame-pointer])

      AS_IF([test "x$enable_shared" != "xyes"],
            [MPTCPD_ADD_COMPILE_FLAG([-static-liblsan])])

      dnl Explicitly check for liblsan through AC_CHECK_LIB instead
      dnl of AC_SEARCH_LIBS since we're relying on the existence of
      dnl the _init symbol found in all libraries.  We also don't
      dnl want to explicitly add -llsan to the link command line
      dnl since the compiler driver will do that automatically.
      AC_CHECK_LIB(
        [lsan],
        [_init],
        [],
        [AC_MSG_ERROR([liblsan is needed for memory error detector])])
    ])
  ])

# MPTCPD_ADD_UBSAN_FLAG
#
# Add support for enabling GCC/Clang undefined behavior detector
# (UndefinedBehaviorSanitizer) instrumentation.
#
# @todo Reduce code duplication between the
#       MPTCPD_ADD_{A,L,UB}SAN_SUPPORT macros.
AC_DEFUN([MPTCPD_ADD_UBSAN_SUPPORT],
  [
   AC_ARG_ENABLE([ubsan],
                 [AS_HELP_STRING([--enable-ubsan],
                                 [Enable undefined behavior detector])],
                 [],
                 [enable_ubsan=no])

   AS_IF([test "x$enable_ubsan" = "xyes"],
     [
      MPTCPD_ADD_COMPILE_FLAG(
        [-fsanitize=undefined],
        [AC_MSG_ERROR([Address sanitizer not supported by the compiler])])

      dnl -fno-omit-frame-pointer to get nicer stack traces.
      MPTCPD_ADD_COMPILE_FLAG([-fno-omit-frame-pointer])

      AS_IF([test "x$enable_shared" != "xyes"],
            [MPTCPD_ADD_COMPILE_FLAG([-static-libubsan])])

      dnl Explicitly check for libubsan through AC_CHECK_LIB instead
      dnl of AC_SEARCH_LIBS since we're relying on the existence of
      dnl the _init symbol found in all libraries.  We also don't
      dnl want to explicitly add -lubsan to the link command line
      dnl since the compiler driver will do that automatically.
      AC_CHECK_LIB(
        [ubsan],
        [_init],
        [],
        [AC_MSG_ERROR([libubsan is needed for memory error detector])])
    ])
  ])
