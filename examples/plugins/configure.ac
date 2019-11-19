# SPDX-License-Identifier: BSD-3-Clause
#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.
#
# Copyright (c) 2019, Intel Corporation

AC_PREREQ([2.69])
AC_INIT([foo_plugin], [0.1])

AM_INIT_AUTOMAKE([foreign])
LT_INIT([disable-static])

AC_CONFIG_SRCDIR([foo.c])
AC_CONFIG_MACRO_DIRS([m4])

# ---------------------------------------------------------------
# Checks for programs.
# ---------------------------------------------------------------
AC_PROG_CC_STDC
AM_PROG_CC_C_O

# ---------------------------------------------------------------
# Checks for libraries.
# ---------------------------------------------------------------
# Find mptcpd.
PKG_CHECK_MODULES([MPTCPD], [mptcpd])

# Determine default mptcpd plugin directory.
PKG_CHECK_VAR([MPTCPD_PLUGINDIR], [mptcpd], [plugindir])

# ---------------------------------------------------------------
# Generate our build files.
# ---------------------------------------------------------------
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
