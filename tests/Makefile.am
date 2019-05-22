## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2019, Intel Corporation

## @todo Do not add the EXECUTABLE_* flags to test libraries (if any).
AM_CPPFLAGS = -I$(top_srcdir)/include
AM_CFLAGS   = $(ELL_CFLAGS) $(EXECUTABLE_CFLAGS)
AM_LDFLAGS  = $(EXECUTABLE_LDFLAGS)

## Build a test mptcpd plugin.
##
## "check_LTLIBRARIES" can't be used here since a dynamically loadable
## module will not be built.  A Libtool convenience library would be
## built instead.
##
## Absolute path needed by Libtool during installation.
plugin_adir    = $(abs_builddir)/plugins_a
plugin_bdir    = $(abs_builddir)/plugins_b
plugin_noopdir = $(abs_builddir)/plugins_noop

## For testing mptcpd plugin framework when default plugin is
## explicitly specified, as well as plugin security checks.
plugin_a_LTLIBRARIES = plugin_three.la plugin_four.la

## For testing mptcpd plugin framework when default plugin is
## is the first registered plugin, i.e. the one with the *lowest*
## plugin priority, as well as testing plugin operation dispatch.
plugin_b_LTLIBRARIES = plugin_one.la plugin_two.la

## For mptcpd tests that require a plugin without testing the plugin
## itself.  The plugin will be a no-op in this case.
plugin_noop_LTLIBRARIES = plugin_noop.la

TEST_PLUGIN_DIR_A    = $(DESTDIR)$(plugin_adir)
TEST_PLUGIN_DIR_B    = $(DESTDIR)$(plugin_bdir)
TEST_PLUGIN_DIR_NOOP = $(DESTDIR)$(plugin_noopdir)

## List of test plugin directory for use as dependencies for test
## plugin installation.
TEST_PLUGIN_DIRS =		\
	$(TEST_PLUGIN_DIR_A)	\
	$(TEST_PLUGIN_DIR_B)	\
	$(TEST_PLUGIN_DIR_NOOP)

## Plugin names
TEST_PLUGIN_ONE     = plugin_one
TEST_PLUGIN_TWO     = plugin_two
TEST_PLUGIN_THREE   = plugin_three
TEST_PLUGIN_FOUR    = plugin_four
TEST_PLUGIN_NOOP    = plugin_noop

## libmptcpd hasn't been installed yet, so directly link to the
## library in its build directory so that dynamic linker can find it
## when relinking the "installed" test plugin binaries.
PLUGIN_LIBMPTCPD_FLAGS = -L$(abs_top_builddir)/lib/.libs -lmptcpd

plugin_one_la_SOURCES  = plugin_one.c
plugin_one_la_CPPFLAGS = \
	-DTEST_PLUGIN=\"$(TEST_PLUGIN_ONE)\" \
	-I$(top_srcdir)/include
plugin_one_la_CFLAGS   = $(ELL_CFLAGS)
plugin_one_la_LDFLAGS  = -no-undefined -module -avoid-version $(ELL_LIBS)
plugin_one_la_LIBADD   = $(PLUGIN_LIBMPTCPD_FLAGS)

plugin_two_la_SOURCES  = plugin_two.c
plugin_two_la_CPPFLAGS = \
	-DTEST_PLUGIN=\"$(TEST_PLUGIN_TWO)\" \
	-I$(top_srcdir)/include
plugin_two_la_CFLAGS   = $(ELL_CFLAGS)
plugin_two_la_LDFLAGS  = -no-undefined -module -avoid-version $(ELL_LIBS)
plugin_two_la_LIBADD   = $(PLUGIN_LIBMPTCPD_FLAGS)

plugin_three_la_SOURCES  = plugin_three.c
plugin_three_la_CPPFLAGS = \
	-DTEST_PLUGIN=\"$(TEST_PLUGIN_THREE)\" \
	-I$(top_srcdir)/include
plugin_three_la_CFLAGS   = $(ELL_CFLAGS)
plugin_three_la_LDFLAGS  = -no-undefined -module -avoid-version $(ELL_LIBS)
plugin_three_la_LIBADD   = $(PLUGIN_LIBMPTCPD_FLAGS)

plugin_four_la_SOURCES  = plugin_four.c
plugin_four_la_CPPFLAGS = \
	-DTEST_PLUGIN=\"$(TEST_PLUGIN_FOUR)\" \
	-I$(top_srcdir)/include
plugin_four_la_CFLAGS   = $(ELL_CFLAGS)
plugin_four_la_LDFLAGS  = \
	-no-undefined -module -avoid-version $(ELL_LIBS)
plugin_four_la_LIBADD   = $(PLUGIN_LIBMPTCPD_FLAGS)

plugin_noop_la_SOURCES  = plugin_noop.c
plugin_noop_la_CPPFLAGS = \
	-DTEST_PLUGIN=\"$(TEST_PLUGIN_NOOP)\" \
	-I$(top_srcdir)/include
plugin_noop_la_CFLAGS   = $(ELL_CFLAGS)
plugin_noop_la_LDFLAGS  = \
	-no-undefined -module -avoid-version $(ELL_LIBS)
plugin_noop_la_LIBADD   = $(PLUGIN_LIBMPTCPD_FLAGS)

## List of test plugin sources for use as dependencies for test plugin
## installation.
TEST_PLUGIN_SOURCE_FILES =		\
	$(plugin_one_la_SOURCES)	\
	$(plugin_two_la_SOURCES)	\
	$(plugin_three_la_SOURCES)	\
	$(plugin_four_la_SOURCES)	\
	$(plugin_noop_la_SOURCES)

check_HEADERS = test-plugin.h

check_PROGRAMS =		\
	test-plugin		\
	test-network-monitor	\
	test-path-manager	\
	test-commands		\
	test-configuration

## Test scripts expected to fail due to intentionally bad mptcpd
## command line options.  Successful exit will be treated as failure.
xfail_test_scripts =		\
	test-bad-log-empty	\
	test-bad-log-long	\
	test-bad-log-short	\
	test-bad-option		\
	test-bad-path-manager	\
	test-bad-plugin-dir

dist_check_SCRIPTS = test-start-stop $(xfail_test_scripts)

test_plugin_SOURCES = test-plugin.c
test_plugin_CPPFLAGS =					\
	-I$(top_srcdir)/include				\
	-D_POSIX_C_SOURCE=200809L			\
	-DTEST_PLUGIN_DIR_A=\"$(TEST_PLUGIN_DIR_A)\"	\
	-DTEST_PLUGIN_DIR_B=\"$(TEST_PLUGIN_DIR_B)\"	\
	-DTEST_PLUGIN_ONE=\"$(TEST_PLUGIN_ONE)\"	\
	-DTEST_PLUGIN_TWO=\"$(TEST_PLUGIN_TWO)\"	\
	-DTEST_PLUGIN_FOUR=\"$(TEST_PLUGIN_FOUR)\"
test_plugin_LDADD = $(top_builddir)/lib/libmptcpd.la $(ELL_LIBS)

test_network_monitor_SOURCES = test-network-monitor.c
test_network_monitor_LDADD = $(top_builddir)/lib/libmptcpd.la $(ELL_LIBS)

test_path_manager_SOURCES = test-path-manager.c
test_path_manager_CPPFLAGS =				\
	$(AM_CPPFLAGS) 					\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_NOOP)\"
test_path_manager_LDADD =			\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)

test_commands_SOURCES = test-commands.c
test_commands_CPPFLAGS =				\
	$(AM_CPPFLAGS) 					\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_NOOP)\"
test_commands_LDADD =				\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)

test_configuration_SOURCES = test-configuration.c
test_configuration_LDADD =				\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)

if HAVE_CXX
check_PROGRAMS += test-cxx-build
test_cxx_build_SOURCES  = test-cxx-build.cpp
test_cxx_build_CPPFLAGS =				\
	-I$(top_srcdir)/include				\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_A)\"	\
	-DTEST_PLUGIN_FOUR=\"$(TEST_PLUGIN_FOUR)\"
test_cxx_build_CXXFLAGS = $(AM_CFLAGS)
test_cxx_build_LDADD = $(top_builddir)/lib/libmptcpd.la $(ELL_LIBS)
endif

## "Install" test plugins, as needed, prior to running the tests.
all-local: $(TEST_PLUGIN_DIRS)

$(TEST_PLUGIN_DIRS): $(TEST_PLUGIN_SOURCE_FILES)
	$(MAKE) $(AM_MAKEFLAGS) install-data

clean-local:
	-rm -rf $(TEST_PLUGIN_DIRS)

AM_TESTS_ENVIRONMENT = TEST_PLUGIN_DIR=$(TEST_PLUGIN_DIR_NOOP)
TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)
XFAIL_TESTS = $(xfail_test_scripts)
