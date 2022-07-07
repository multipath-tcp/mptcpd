// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file tests/lib/interface.h
 *
 * @brief @c mptcpd_interface related utility functions.
 *
 * Copyright (c) 2022, Intel Corporation
 */

#include <stdbool.h>


struct mptcpd_interface;
struct sockaddr;

/// Create a @c struct @c mptcpd_interface for test purposes.
struct mptcpd_interface *test_mptcpd_interface_create(void);

/**
 * @brief Add an IP address to the address list in @a i.
 *
 * @param[in,out] i  mptcpd interface information to which the IP
 *                   address @a sa will be added.
 * @param[in]     sa IP address to be add to the address list in
 *                   @a i.
 *
 * @return @c true on success, @c false otherwise.
 */
bool test_mptcpd_interface_insert_addr(struct mptcpd_interface *i,
                                       struct sockaddr const *sa);

/// Destroy the @c struct @c mptcpd_interface instance @a i.
void test_mptcpd_interface_destroy(struct mptcpd_interface *i);


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
