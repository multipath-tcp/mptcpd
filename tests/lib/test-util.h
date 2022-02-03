// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file test-util.h
 *
 * @brief mptcpd test utilities library.
 *
 * Copyright (c) 2020, 2022, Intel Corporation
 */

#ifndef MPTCP_TEST_UTIL_H
#define MPTCP_TEST_UTIL_H


/// Get MPTCP path management generic netlink API family name.
char const *tests_get_pm_family_name(void);

/**
 * @brief Exit test process if the kernel does not support MPTCP.
 *
 * If the kernel does not support MPTCP exit the current test process
 * with an exit status suitable for making the Automake @c test-driver
 * script interpret the test result as @c SKIP instead of @c PASS or
 * @c FAIL.  This is useful for tests that need MPTCP but are run on
 * platforms with a non-MPTCP capable kernel.
 */
void tests_skip_if_no_mptcp(void);


#endif  // MPTCP_TEST_UTIL_H
