# SPDX-License-Identifier: BSD-3-Clause
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Copyright (c) 2017-2021, Intel Corporation

AC_PREREQ([2.69])
AC_INIT([mptcpd],
        [0.6],
        [mptcp@lists.linux.dev],
        [],
        [https://01.org/multipath-tcp-linux])

# ---------------------------------------------------------------
# Libtool Based Library Versioning
# ---------------------------------------------------------------
# Source     changed:  REVISION++
# Interfaces changed:  CURRENT++ REVISION=0
#            added:    CURRENT++ REVISION=0 AGE++
#            removed:  CURRENT++ REVISION=0 AGE=0
LIB_CURRENT=1
LIB_REVISION=0
LIB_AGE=0

AC_SUBST([LIB_CURRENT])
AC_SUBST([LIB_REVISION])
AC_SUBST([LIB_AGE])
# ---------------------------------------------------------------

dnl Support "--enable-debug=..." configure script command line option.
AX_IS_RELEASE([git-directory])
AX_CHECK_ENABLE_DEBUG([yes])

AM_INIT_AUTOMAKE([1.15 -Wall -Werror -Wno-portability silent-rules std-options check-news])
AM_SILENT_RULES([yes])
LT_INIT([disable-static])

AC_CONFIG_SRCDIR([src/mptcpd.c])
AC_CONFIG_HEADERS([include/mptcpd/private/config.h])

AC_CONFIG_MACRO_DIR([m4])

# ---------------------------------------------------------------
# Checks for programs.
# ---------------------------------------------------------------
AC_PROG_CC_STDC
AM_PROG_CC_C_O

AC_PROG_CXX
AM_CONDITIONAL([HAVE_CXX], [test -n "$CXX"])

# Gcov/lcov support
AX_CODE_COVERAGE

# Work around break in AX_CODE_COVERAGE backward compatibility.
AX_ADD_AM_MACRO_STATIC([@CODE_COVERAGE_RULES@])
AC_CONFIG_COMMANDS_PRE([
  AC_SUBST([CODE_COVERAGE_RULES])
])

# Check for Doxygen
DX_DOXYGEN_FEATURE(ON)
DX_DOT_FEATURE(ON)
DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([mptcpd], [Doxyfile], [doc])

# Check for pandoc
AC_ARG_VAR([PANDOC],[location of pandoc program])
AC_PATH_PROG([PANDOC], [pandoc])

# Enable pkg-config
PKG_PROG_PKG_CONFIG

# Enable address sanitizer support.
MPTCPD_ADD_ASAN_SUPPORT

# Enable leak sanitizer support.
MPTCPD_ADD_LSAN_SUPPORT

# Enable undefined behavior sanitizer support.
MPTCPD_ADD_UBSAN_SUPPORT

# Allow the user to set the log message destination at compile-time.
# The available destinations correspond to those made available by
# ELL.
AC_ARG_ENABLE([logging],
     [AS_HELP_STRING([--enable-logging[=DEST]],
                     [Enable logging to DEST (journal, stderr, syslog) @<:@default=stderr@:>@])],
     [AS_CASE([$enableval],
              [journal | stderr | syslog],
              [enable_logging=$enableval],
              [yes],
              [enable_logging=stderr],
              [no],
              [enable_logging=null],
              [AC_MSG_ERROR([invalid logging destination value: $enableval])])],
     [enable_logging=stderr])

AC_DEFINE_UNQUOTED([MPTCPD_LOGGER],
                   [$enable_logging],
                   [Log message destination.])
AC_SUBST([mptcpd_logger],[$enable_logging])

# Allow the user to set the default path manager plugin at 'configure-time'.
AC_ARG_WITH([path-manager],
     [AS_HELP_STRING([--with-path-manager[=PLUGIN]],
                     [Set default path manager plugin to PLUGIN (addr_adv, sspi) @<:@default=addr_adv@:>@])],
     [AS_CASE([$withval],
              [addr_adv | sspi],
              [with_path_manager=$withval],
              [AC_MSG_ERROR([invalid path manager plugin: $withval])])],
     [with_path_manager=addr_adv])

AC_SUBST([mptcpd_default_pm],[$with_path_manager])

# Systemd unit directory detection and handling.
AC_ARG_WITH([systemdsystemunitdir],
     [AS_HELP_STRING([--with-systemdsystemunitdir=DIR],
                     [Directory for systemd service files])],,
     [with_systemdsystemunitdir=auto])

AS_IF([test "x$with_systemdsystemunitdir" = "xyes" -o "x$with_systemdsystemunitdir" = "xauto"],
      [def_systemdsystemunitdir=$($PKG_CONFIG --variable=systemdsystemunitdir systemd)
       AS_IF([test "x$def_systemdsystemunitdir" = "x"],
             [AS_IF([test "x$with_systemdsystemunitdir" = "xyes"],
                    [AC_MSG_ERROR([systemd support requested but pkg-config unable to query systemd package])])
              with_systemdsystemunitdir=no],
             [with_systemdsystemunitdir="$def_systemdsystemunitdir"])])

AS_IF([test "x$with_systemdsystemunitdir" != "xno"],
      [AC_SUBST([systemdsystemunitdir], [$with_systemdsystemunitdir])
       AC_MSG_NOTICE([systemd system unit directory: $with_systemdsystemunitdir])])

AM_CONDITIONAL([HAVE_SYSTEMD], [test "x$with_systemdsystemunitdir" != "xno"])

# Adjust optimization flags depending on debug configuration.
AS_IF([test "x$ax_enable_debug" != "xyes"],
      [
       dnl Explicitly add compile-time optimization flag since
       dnl AX_ENABLE_DEBUG macro disables the autoconf default of
       dnl implicitly adding it.
       AS_IF([test "x$enable_code_coverage" != "xyes"],
             [AX_APPEND_COMPILE_FLAGS([-O2])

              AC_LANG_PUSH([C++])
              AX_APPEND_COMPILE_FLAGS([-O2])
              AC_LANG_POP([C++])
             ])

       dnl AX_CODE_COVERAGE will define NDEBUG on the command line,
       dnl equivalent to #define NDEBUG 1, if code coverage is
       dnl enabled.  Work around old versions of
       dnl AX_CHECK_ENABLE_DEBUG() that define NDEBUG in the config
       dnl header without a value, resulting in conflicting
       dnl definitions.  Explicitly set NDEBUG=1 in the config header
       dnl to force the definitions to be the same.
       AC_DEFINE([NDEBUG], [1])
      ],
      [
       dnl Minimal optimization for the debug case.  We need this for
       dnl _FORTIFY_SOURCE support, too.
       AX_APPEND_COMPILE_FLAGS([-Og])

       dnl Enable debug optimization for C++ build test, too.
       AS_IF([test -n "$CXX"],
             [AC_LANG_PUSH([C++])
              AX_APPEND_COMPILE_FLAGS([-Og])
              AC_LANG_POP([C++])
             ])
      ])


dnl Export symbols in the public API from shared libraries.
AM_CONDITIONAL([BUILDING_DLL], [test "x$enable_shared" = xyes])

# ---------------------------------------------------------------
# Checks for libraries.
# ---------------------------------------------------------------
ELL_VERSION=0.27  dnl Minimum required version of ELL.
PKG_CHECK_MODULES([ELL],
                  [ell >= $ELL_VERSION])
AC_SUBST([ELL_VERSION])

# ---------------------------------------------------------------
# Checks for header files.
# ---------------------------------------------------------------
# @todo mptcpd ships with a copies of the upstream and
#       multipath-tcp.org kernel <linux/mptcp.h> header files. Ideally
#       it should not. Consider removing them from mptcpd.
AH_TEMPLATE([HAVE_LINUX_MPTCP_H_UPSTREAM],
            [Define to 1 if you have the upstream kernel
            <linux/mptcp.h> header.])
AH_TEMPLATE([HAVE_LINUX_MPTCP_H_MPTCP_ORG],
            [Define to 1 if you have the multipath-tcp.org kernel
            <linux/mptcp.h> header.])
AH_TEMPLATE([HAVE_LINUX_MPTCP_H_UPSTREAM_EVENTS],
            [Define to 1 if <linux/mptcp.h> supports MPTCP genl events.])
AC_CHECK_HEADERS([linux/mptcp.h],
   [
    AX_CHECK_DEFINE(
        [linux/mptcp.h],
        [MPTCP_PM_NAME],
        [AC_DEFINE([HAVE_LINUX_MPTCP_H_UPSTREAM], [1])
         AX_CHECK_DEFINE([linux/mptcp.h],
                         [MPTCP_PM_EV_GRP_NAME],
                         [AC_DEFINE([HAVE_LINUX_MPTCP_H_UPSTREAM_EVENTS],
                                    [1])
                         ])
        ],
        [
         AX_CHECK_DEFINE(
             [linux/mptcp.h],
             [MPTCP_GENL_NAME],
             [AC_DEFINE([HAVE_LINUX_MPTCP_H_MPTCP_ORG], [1])],
             [AC_MSG_ERROR([Discrepancy in <linux/mptcp.h>.])])
        ])
   ],
   [AC_MSG_WARN([<linux/mptcp.h> header not found. Using fallback.])])

# ---------------------------------------------------------------
# Checks for typedefs, structures, and compiler characteristics.
# ---------------------------------------------------------------
AX_APPEND_COMPILE_FLAGS([-fvisibility=hidden])

# ---------------------------------------------------------------
# Checks for library functions.
# ---------------------------------------------------------------
AC_SEARCH_LIBS(
    [argp_parse],
    [argp],
    [],
    [AC_MSG_ERROR([library with argp_parse() could not be found])])

AC_SEARCH_LIBS(
    [dlopen],
    [dl],
    [],
    [AC_MSG_ERROR([library with dlopen() could not be found])])

# ---------------------------------------------------------------
# Enable additional C compiler warnings.  We do this after all
# Autoconf tests have been run since not all autoconf macros are
# warning free.
# ---------------------------------------------------------------
AX_CFLAGS_WARN_ALL([CFLAGS])
AX_APPEND_COMPILE_FLAGS([-Wextra -Werror -pedantic])

# ---------------------------------------------------------------
# Enable compile-time defense
# ---------------------------------------------------------------
# Fortify source
# Enabling optimization implies _FORTIFY_SOURCE on some platforms.
# Explicitly redefine to _FORTIFY_SOURCE=2 to make sure we have the
# desired fortification level.
AX_APPEND_FLAG([-U_FORTIFY_SOURCE], [CPPFLAGS])
AX_APPEND_FLAG([-D_FORTIFY_SOURCE=2], [CPPFLAGS])

# Format string vulnerabilities
# -Wformat=2 implies:
#    -Wformat -Wformat-nonliteral -Wformat-security -Wformat-y2k
AX_APPEND_COMPILE_FLAGS([-Wformat=2])

# Stack-based buffer overrun detection
MPTCPD_ADD_COMPILE_FLAG([-fstack-protector-strong],
                        [# GCC < 4.9
                         MPTCPD_ADD_COMPILE_FLAG([-fstack-protector])
                        ])

# Position Independent Execution (PIE)
AX_APPEND_COMPILE_FLAGS([-fPIE], [EXECUTABLE_CFLAGS])
AC_SUBST([EXECUTABLE_CFLAGS])


# ---------------------------------------------------------------
# Enable link-time defenses
# ---------------------------------------------------------------
# Stack execution protection
AX_APPEND_LINK_FLAGS([-Wl,-z,noexecstack])

# Data relocation and protection (RELRO)
AX_APPEND_LINK_FLAGS([-Wl,-z,now -Wl,-z,relro])

# Position Independent Execution
AX_APPEND_LINK_FLAGS([-pie], [EXECUTABLE_LDFLAGS])
AC_SUBST([EXECUTABLE_LDFLAGS])

# ---------------------------------------------------------------
# Test plugin names (centrally defined here)
# ---------------------------------------------------------------
AC_SUBST([TEST_PLUGIN_ONE],   [plugin_one])
AC_SUBST([TEST_PLUGIN_TWO],   [plugin_two])
AC_SUBST([TEST_PLUGIN_THREE], [plugin_three])
AC_SUBST([TEST_PLUGIN_FOUR],  [plugin_four])
AC_SUBST([TEST_PLUGIN_NOOP],  [plugin_noop])

# ---------------------------------------------------------------
# Generate our build files.
# ---------------------------------------------------------------
AC_CONFIG_FILES([Makefile
                 etc/Makefile
                 man/Makefile
                 include/Makefile
                 include/linux/Makefile
                 include/mptcpd/Makefile
                 lib/Makefile
                 lib/mptcpd.pc
                 src/Makefile
                 plugins/Makefile
                 plugins/path_managers/Makefile
                 scripts/Makefile
                 tests/Makefile
                 tests/lib/Makefile
                 tests/plugins/Makefile
                 tests/plugins/noop/Makefile
                 tests/plugins/priority/Makefile
                 tests/plugins/security/Makefile])
AC_OUTPUT
