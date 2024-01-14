## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2021, Intel Corporation

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
## explicitly specified, as well as plugin security checks.
check_LTLIBRARIES = three.la four.la

three_la_SOURCES = three.c
three_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-DTEST_PLUGIN=\"@TEST_PLUGIN_THREE@\"
three_la_LIBADD =				\
	../../lib/libmptcpd_test.la		\
	$(top_builddir)/lib/libmptcpd.la	\
	$(CODE_COVERAGE_LIBS)

four_la_SOURCES = four.c
four_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-DTEST_PLUGIN=\"@TEST_PLUGIN_FOUR@\"
four_la_LIBADD =				\
	../../lib/libmptcpd_test.la		\
	$(top_builddir)/lib/libmptcpd.la	\
	$(CODE_COVERAGE_LIBS)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean

# Force check_LTLIBRARIES to be built on check-valgrind.
check-valgrind-local: check
