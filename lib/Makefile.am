## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2018-2020, Intel Corporation

include $(top_srcdir)/aminclude_static.am

if BUILDING_DLL
AM_CPPFLAGS = -DMPTCPD_DLL -DMPTCPD_DLL_EXPORTS
endif

lib_LTLIBRARIES = libmptcpd.la

libmptcpd_la_CPPFLAGS =						\
	-I$(top_srcdir)/include -I$(top_builddir)/include	\
	$(CODE_COVERAGE_CPPFLAGS) $(AM_CPPFLAGS)
libmptcpd_la_CFLAGS = $(ELL_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libmptcpd_la_LIBADD = $(ELL_LIBS) $(CODE_COVERAGE_LIBS)
libmptcpd_la_LDFLAGS = 	\
	-version-info @LIB_CURRENT@:@LIB_REVISION@:@LIB_AGE@

libmptcpd_la_SOURCES =		\
	addr_info.c		\
	configuration.c		\
	id_manager.c		\
	network_monitor.c	\
	path_manager.c		\
	plugin.c		\
	sockaddr.c

#if BUILDING_DLL
#libmptcpd_la_SOURCES += debug.c
#endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = mptcpd.pc

clean-local: code-coverage-clean
