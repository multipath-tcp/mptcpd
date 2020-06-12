## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2020, Intel Corporation

include $(top_srcdir)/aminclude_static.am

AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(top_srcdir)/tests/lib	\
	$(CODE_COVERAGE_CPPFLAGS)

AM_CFLAGS = $(ELL_CFLAGS) $(CODE_COVERAGE_CFLAGS)

## -rpath is needed to force a DSO to be built when listing Libtool
## modules in the Automake check_LTLIBRARIES variable.  Otherwise a
## convenience library would be built instead.
AM_LDFLAGS =			\
	-no-undefined		\
	-module			\
	-avoid-version		\
	$(ELL_LIBS)		\
	-rpath $(abs_builddir)

## For testing mptcpd plugin framework when default plugin is
## is the first registered plugin, i.e. the one with the *lowest*
## plugin priority, as well as testing plugin operation dispatch.
check_LTLIBRARIES = one.la two.la

one_la_SOURCES = one.c
one_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-DTEST_PLUGIN=\"@TEST_PLUGIN_ONE@\"
one_la_LIBADD =					\
	../../lib/libmptcpd_test.la		\
	$(top_builddir)/lib/libmptcpd.la	\
	$(CODE_COVERAGE_LIBS)

two_la_SOURCES = two.c
two_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-DTEST_PLUGIN=\"@TEST_PLUGIN_TWO@\"
two_la_LIBADD  =				\
	../../lib/libmptcpd_test.la		\
	$(top_builddir)/lib/libmptcpd.la	\
	$(CODE_COVERAGE_LIBS)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean
