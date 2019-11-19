## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2019, Intel Corporation

ACLOCAL_AMFLAGS = -I m4

plugindir = @MPTCPD_PLUGINDIR@
plugin_LTLIBRARIES = foo.la

foo_la_SOURCES = foo.c
foo_la_CFLAGS  = $(MPTCPD_PLUGIN_CFLAGS)
foo_la_LDFLAGS = -no-undefined -module -avoid-version
foo_la_LIBADD  = $(MPTCPD_LIBS)
