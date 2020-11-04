## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2019, Intel Corporation

include $(top_srcdir)/aminclude_static.am

AM_CFLAGS   = $(ELL_CFLAGS) $(EXECUTABLE_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	$(CODE_COVERAGE_CPPFLAGS)

## Expose an internal convenience library for testing purposes.
noinst_LTLIBRARIES = libpath_manager.la

libpath_manager_la_SOURCES =	\
	commands.c		\
	commands.h		\
	commands_mptcp_org.c	\
	configuration.c		\
	configuration.h		\
	path_manager.c		\
	path_manager.h

libpath_manager_la_LIBADD =			\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS) $(CODE_COVERAGE_LIBS)

libpath_manager_la_CPPFLAGS =	\
	$(AM_CPPFLAGS)		\
	-DMPTCPD_CONFIG_FILE='"$(sysconfdir)/@PACKAGE@/@PACKAGE@.conf"'

EXTRA_DIST = mptcp.service.in

if HAVE_SYSTEMD
systemdsystemunit_DATA = mptcp.service
libexec_PROGRAMS = mptcpd

## The configure script won't fully expand $libexecdir so leverage
## `make' based variable expansion instead.
mptcp.service: Makefile mptcp.service.in
	$(AM_V_GEN)rm -f $@ $@.tmp; \
	srcdir=''; \
		test -f ./$@.in || srcdir=$(srcdir)/; \
		sed \
			-e 's,@libdir[@],$(libdir),g' \
			-e 's,@libexecdir[@],$(libexecdir),g' \
			$${srcdir}$@.in >$@.tmp; \
	chmod 644 $@.tmp; \
	mv $@.tmp $@

CLEANFILES = mptcp.service
else
bin_PROGRAMS = mptcpd
endif

mptcpd_SOURCES  = mptcpd.c
mptcpd_LDADD    =				\
	$(builddir)/libpath_manager.la		\
	$(ELL_LIBS) $(CODE_COVERAGE_LIBS)
mptcpd_LDFLAGS  = $(EXECUTABLE_LDFLAGS)

clean-local: code-coverage-clean
