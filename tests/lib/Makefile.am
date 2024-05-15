## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2019-2020, 2022, Intel Corporation

include $(top_srcdir)/aminclude_static.am

AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	$(CODE_COVERAGE_CPPFLAGS)
AM_CFLAGS = $(ELL_CFLAGS) $(CODE_COVERAGE_CFLAGS)

## Convenience library that contains utility functions used by the
## mptcpd tests.
check_LTLIBRARIES = libmptcpd_test.la

libmptcpd_test_la_SOURCES =	\
	call_count.c		\
	call_plugin.c		\
	interface.c		\
	interface.h		\
	sockaddr.c		\
	test-plugin.h		\
	test-util.c		\
	test-util.h
libmptcpd_test_la_LIBADD =			\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean
