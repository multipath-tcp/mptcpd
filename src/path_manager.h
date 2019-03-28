// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file src/path_manager.h
 *
 * @brief mptcpd user space path manager header file (internal).
 *
 * Copyright (c) 2017-2019, Intel Corporation
 */

#ifndef MPTCPD_PATH_MANAGER_H
#define MPTCPD_PATH_MANAGER_H


struct mptcpd_pm;
struct mptcpd_config;

/**
 * @brief Create a path manager.
 *
 * @param[in] config Mptcpd configuration.
 *
 * @todo As currently implemented, one could create multiple path
 *       manager instances?  Is that useful?
 *
 * @return Pointer to new path manager on success.  @c NULL on
 *         failure.
 */
struct mptcpd_pm *
mptcpd_pm_create(struct mptcpd_config const *config);

/**
 * Destroy a path manager.
 *
 * @param[in,out] pm Path manager to be destroyed.
 */
void mptcpd_pm_destroy(struct mptcpd_pm *pm);


#endif /* MPTCPD_PATH_MANAGER_H */


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
