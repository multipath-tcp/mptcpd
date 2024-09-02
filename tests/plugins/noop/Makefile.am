## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2021, Intel Corporation

include $(top_srcdir)/aminclude_static.am

AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
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

## For mptcpd tests that require a plugin without testing the plugin
## itself.  The plugin will be a no-op in this case.
check_LTLIBRARIES = noop.la

noop_la_SOURCES = noop.c
noop_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)				\
	-DTEST_PLUGIN=\"@TEST_PLUGIN_NOOP@\"
noop_la_LIBADD = $(top_builddir)/lib/libmptcpd.la $(CODE_COVERAGE_LIBS)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean

# Force check_LTLIBRARIES to be built on check-valgrind.
check-valgrind-local: check
