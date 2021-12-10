## SPDX-License-Identifier: BSD-3-Clause
##
## Copyright (c) 2017-2021, Intel Corporation

include $(top_srcdir)/aminclude_static.am

SUBDIRS = lib plugins

## @todo Do not add the EXECUTABLE_* flags to test libraries (if any).
AM_CPPFLAGS =				\
	-I$(top_srcdir)/include		\
	-I$(top_builddir)/include	\
	-I$(srcdir)/lib			\
	$(CODE_COVERAGE_CPPFLAGS)
AM_CFLAGS   = $(ELL_CFLAGS) $(EXECUTABLE_CFLAGS) $(CODE_COVERAGE_CFLAGS)
AM_CXXFLAGS = $(ELL_CFLAGS) $(EXECUTABLE_CFLAGS) $(CODE_COVERAGE_CXXFLAGS)
AM_LDFLAGS  = $(EXECUTABLE_LDFLAGS)

TEST_PLUGIN_DIR_SECURITY = $(abs_builddir)/plugins/security/.libs
TEST_PLUGIN_DIR_PRIORITY = $(abs_builddir)/plugins/priority/.libs
TEST_PLUGIN_DIR_NOOP     = $(abs_builddir)/plugins/noop/.libs

check_PROGRAMS =		\
	test-plugin		\
	test-network-monitor	\
	test-path-manager	\
	test-commands		\
	test-configuration	\
	test-id-manager		\
	test-sockaddr		\
	test-addr-info

noinst_PROGRAMS = mptcpwrap-tester

dist_check_SCRIPTS =		\
	test-bad-log-empty	\
	test-bad-log-long	\
	test-bad-log-short	\
	test-bad-option		\
	test-bad-path-manager	\
	test-bad-plugin-dir	\
	test-start-stop		\
	test-mptcpwrap

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
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_network_monitor_SOURCES = test-network-monitor.c
test_network_monitor_LDADD =			\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_path_manager_SOURCES = test-path-manager.c
test_path_manager_CPPFLAGS =				\
	$(AM_CPPFLAGS) 					\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_NOOP)\"
test_path_manager_LDADD =			\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_commands_SOURCES = test-commands.c
test_commands_CPPFLAGS =				\
	$(AM_CPPFLAGS) 					\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_NOOP)\"
test_commands_LDADD =				\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_configuration_SOURCES = test-configuration.c
test_configuration_LDADD =			\
	$(top_builddir)/src/libpath_manager.la	\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_id_manager_SOURCES = test-id-manager.c
test_id_manager_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_sockaddr_SOURCES = test-sockaddr.c
test_sockaddr_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

test_addr_info_SOURCES = test-addr-info.c
test_addr_info_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)

mptcpwrap_tester_SOURCES = mptcpwrap-tester.c
mptcpwrap_tester_LDADD   = $(CODE_COVERAGE_LIBS)

if HAVE_CXX
check_PROGRAMS += test-cxx-build
test_cxx_build_SOURCES  = test-cxx-build.cpp
test_cxx_build_CPPFLAGS =					\
	$(AM_CPPFLAGS)						\
	-DTEST_PLUGIN_DIR=\"$(TEST_PLUGIN_DIR_SECURITY)\"	\
	-DTEST_PLUGIN_FOUR=\"@TEST_PLUGIN_FOUR@\"
test_cxx_build_LDADD =				\
	$(top_builddir)/lib/libmptcpd.la	\
	$(builddir)/lib/libmptcpd_test.la	\
	$(ELL_LIBS)				\
	$(CODE_COVERAGE_LIBS)
endif

AM_TESTS_ENVIRONMENT = TEST_PLUGIN_DIR=$(TEST_PLUGIN_DIR_NOOP)
TESTS = $(check_PROGRAMS) $(dist_check_SCRIPTS)

# Clean up code coverage related generated files.
clean-local: code-coverage-clean
