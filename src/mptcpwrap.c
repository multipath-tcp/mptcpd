// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpwrap.c
 *
 * @brief wrapper for socket() libcall hijacking
 *
 * Copyright (c) 2021, Red Hat, Inc.
 */

#include <sys/syscall.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <linux/net.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// libtool will make every symbol hidden by default
int __attribute__((visibility("default"))) socket(int family, int type, int protocol)
{
	int ret, orig_protocol = protocol;

	// the 'type' field may encode socket flags
	if ((family != AF_INET && family != AF_INET6) ||
	    (type & 0xff)  != SOCK_STREAM)
		goto do_socket;

	// socket(AF_INET, SOCK_STREM, 0) maps to TCP, too
	if (protocol != 0 && protocol != IPPROTO_TCP)
		goto do_socket;

	protocol = IPPROTO_TCP + 256;

do_socket:
	ret = syscall(__NR_socket, family, type, protocol);
	if (getenv("MPTCPWRAP_DEBUG") && protocol != orig_protocol)
		fprintf(stderr, "mptcpwrap: changing socket protocol from 0x%x "
				"to 0x%x (IPPROTO_MPTCP) for family 0x%x "
				"type 0x%x fd %d\n", orig_protocol, protocol,
				family, type, ret);
	return ret;
}
