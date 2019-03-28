/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */

/*
 * MPTCP generic netlink interface public header.
 *
 * Copyright 2018 Intel Corporation All Rights Reserved.
 *
 * Based on nl80211.h.
 */

#ifndef _UAPI_LINUX_MPTCP_GENL_H
#define _UAPI_LINUX_MPTCP_GENL_H

#include <linux/types.h>

#define MPTCP_GENL_NAME "mptcp"
#define MPTCP_PM_NAME_LEN 16   /* Maximum path manager name length */

#define MPTCP_MULTICAST_GROUP_NEW_CONNECTION	"new_connection"
#define MPTCP_MULTICAST_GROUP_NEW_ADDR		"new_addr"
#define MPTCP_MULTICAST_GROUP_NEW_SUBFLOW	"new_subflow"
#define MPTCP_MULTICAST_GROUP_SUBFLOW_CLOSED	"subflow_closed"
#define MPTCP_MULTICAST_GROUP_CONNECTION_CLOSED	"conn_closed"


/**
 * enum mptcp_genl_commands - Supported mptcp_genl commands.
 *
 * @MPTCP_GENL_CMD_SEND_ADDR: Notify the kernel space of the
 *	availability of new address for use in MPTCP connections.
 *	Triggers an ADD_ADDR to be sent to the peer.
 * @MPTCP_GENL_CMD_ADD_SUBFLOW: Add new subflow to the MPTCP
 *	connection.  Triggers an MP_JOIN to be sent to the peer.
 * @MPTCP_GENL_CMD_SET_BACKUP: Set subflow priority to 'backup'.
 * @MPTCP_GENL_CMD_REMOVE_SUBFLOW: Trigger a REMOVE_ADDR MPTCP option
 *	to be sent, ultimately resulting in subflows routed through
 *	the invalidated address to be closed.
 */
enum mptcp_genl_commands {
	MPTCP_GENL_CMD_SEND_ADDR,
	MPTCP_GENL_CMD_ADD_SUBFLOW,
	MPTCP_GENL_CMD_SET_BACKUP,
	MPTCP_GENL_CMD_REMOVE_SUBFLOW
};

/**
 * enum mptcp_genl_attrs - mptcp_genl netlink attributes.
 *
 * @MPTCP_GENL_ATTR_UNSPEC: internal
 * @MPTCP_GENL_ATTR_CONNECTION_ID: MPTCP connection identifier.
 * @MPTCP_GENL_ATTR_LOCAL_ADDRESS_ID: MPTCP host local address identifer.
 * @MPTCP_GENL_ATTR_REMOTE_ADDRESS_ID: MPTCP host remote address identifer.
 * @MPTCP_GENL_ATTR_LOCAL_IPV4_ADDR: Local IPv4 address.
 * @MPTCP_GENL_ATTR_REMOTE_IPV4_ADDR: Peer IPv4 address.
 * @MPTCP_GENL_ATTR_LOCAL_IPV6_ADDR: Local IPv6 address.
 * @MPTCP_GENL_ATTR_REMOTE_IPV6_ADDR: Peer IPv6 address.
 * @MPTCP_GENL_ATTR_LOCAL_PORT: Local IP port.
 * @MPTCP_GENL_ATTR_REMOTE_PORT: Peer IP port.
 * @MPTCP_GENL_ATTR_BACKUP: MPTCP subflow backup priority flag is set.
 * @MPTCP_GENL_ATTR_PATH_MANAGER: Path management strategy name.
 * @_MPTCP_GENL_ATTR_AFTER_LAST:  internal
 * @NUM_MPTCP_GENL_ATTR: internal
 * @MPTCP_GENL_ATTR_MAX: internal
 */
enum mptcp_genl_attrs {
	MPTCP_GENL_ATTR_UNSPEC,

	MPTCP_GENL_ATTR_CONNECTION_ID,
	MPTCP_GENL_ATTR_LOCAL_ADDRESS_ID,
	MPTCP_GENL_ATTR_REMOTE_ADDRESS_ID,
	MPTCP_GENL_ATTR_LOCAL_IPV4_ADDR,
	MPTCP_GENL_ATTR_REMOTE_IPV4_ADDR,
	MPTCP_GENL_ATTR_LOCAL_IPV6_ADDR,
	MPTCP_GENL_ATTR_REMOTE_IPV6_ADDR,
	MPTCP_GENL_ATTR_LOCAL_PORT,
	MPTCP_GENL_ATTR_REMOTE_PORT,
	MPTCP_GENL_ATTR_BACKUP,
	MPTCP_GENL_ATTR_PATH_MANAGER,

	/* add attributes here, update the policy in path_manager.c */

	_MPTCP_GENL_ATTR_AFTER_LAST,
	NUM_MPTCP_GENL_ATTR = _MPTCP_GENL_ATTR_AFTER_LAST,
	MPTCP_GENL_ATTR_MAX = _MPTCP_GENL_ATTR_AFTER_LAST - 1
};


#endif  /* _UAPI_LINUX_MPTCP_GENL_H */
