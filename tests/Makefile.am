## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2020, Intel Corporation

SUBDIRS = lib plugins

## @todo Do not add the EXECUTABLE_* flags to test libraries (if any).
AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(srcdir)/lib
AM_CFLAGS   = $(ELL_CFLAGS) $(EXECUTABLE_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_LDFLAGS  = $(EXECUTABLE_LDFLAGS)

TEST_PLUGIN_DIR_SECURITY = $(abs_builddir)/plugins/security/.libs
TEST_PLUGIN_DIR_PRIORITY = $(abs_builddir)/plugins/priority/.libs
TEST_PLUGIN_DIR_NOOP     = $(abs_builddir)/plugins/noop/.libs

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
test_plugin_CPPFLAGS =							\
	$(AM_CPPFLAGS)							\
	-D_POSIX_C_SOURCE=200809L					\
	-DTEST_PLUGIN_DIR_SECURITY=\"$(TEST_PLUGIN_DIR_SECURITY)\"	\
	-DTEST_PLUGIN_DIR_PRIORITY=\"$(TEST_PLUGIN_DIR_PRIORITY)\"	\
	-DTEST_PLUGIN_DIR_NOOP=\"$(TEST_PLUGIN_DIR_NOOP)\"		\
	-DTEST_PLUGIN_ONE=\"@TEST_PLUGIN_ONE@\"				\
	-DTEST_PLUGIN_TWO=\"@TEST_PLUGIN_TWO@\"				\
	-DTEST_PLUGIN_FOUR=\"@TEST_PLUGIN_FOUR@\"
test_plugin_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)

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
test_configuration_LDADD =			\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)

if HAVE_CXX
check_PROGRAMS += test-cxx-build
test_cxx_build_SOURCES  = test-cxx-build.cpp
test_cxx_build_CPPFLAGS =					\
	$(AM_CPPFLAGS)						\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_SECURITY)\"	\
	-DTEST_PLUGIN_FOUR=\"@TEST_PLUGIN_FOUR@\"
test_cxx_build_CXXFLAGS = $(AM_CFLAGS) -Og
test_cxx_build_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)
endif

AM_TESTS_ENVIRONMENT = TEST_PLUGIN_DIR=$(TEST_PLUGIN_DIR_NOOP)
TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)
XFAIL_TESTS = $(xfail_test_scripts)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean
