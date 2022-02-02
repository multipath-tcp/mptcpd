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

/**
 * @brief Automake test driver "skip" exit status.
 *
 * An exit status of 77 causes the Automake test driver, i.e. the
 * @c test-driver script, to consider the test as skipped rather than
 * passed or failed.
 *
 * Tests that should be skipped under some conditions, such as when
 * not running a MPTCP capable kernel should exit the process with
 * this value.
 */
#define TESTS_SKIP_EXIT_STATUS 77


/// Get MPTCP path management generic netlink API family name.
char const *tests_get_pm_family_name(void);

/// Is the kernel MPTCP capable?
bool tests_is_mptcp_kernel(void);


#endif  // MPTCP_TEST_UTIL_H
