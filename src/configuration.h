// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file configuration.h
 *
 * @brief Mptcpd configuration parser header.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#ifndef MPTCPD_CONFIGURATION_H
#define MPTCPD_CONFIGURATION_H


/**
 * Function pointer corresponding to the ELL functions that set the
 * underlying logging mechanism, e.g. @c l_log_set_stderr,
 * @c l_log_set_syslog and @c l_log_set_journal.
 */
typedef void (*mptcpd_set_log_func_t)(void);

/**
 * @brief mptcpd configuration parameters
 *
 * This structure defines a set of configuration parameters for
 * mptcpd.  Only one instance of this structure is intended to exist
 * in a given mptcpd process.
 */
struct mptcpd_config
{
        /**
         * @brief Function that configures the ELL logging mechanism.
         *
         * This function is called by mptcpd to configure the
         * underlying logging mechanism.  In particular, it will be
         * any of the following:
         *
         * @li @c l_log_set_null
         * @li @c l_log_set_stderr
         * @li @c l_log_set_syslog
         * @li @c l_log_set_journal
         */
        mptcpd_set_log_func_t log_set;

        /// Location of mptcpd plugins.
        char const *plugin_dir;

        /**
         * @brief Name of default plugin.
         *
         * Use the operations associated with the plugin of this name
         * if a path management strategy wasn't configured for a given
         * MPTCP connection.
         */
        char const *default_plugin;

        /**
         * @brief Kernel/mptcpd state sync interval in seconds.
         *
         * The amount of time in seconds between periodic
         * synchronizations of the kernel and mptcpd state, e.g. MPTCP
         * address ID mappings.
         *
         * @note An coarse grained interval resolution in seconds is
         *       used instead of finer resolutions like milliseconds
         *       since we don't want to have a large amount of
         *       kernel/user-space communication in a short period of
         *       time.
         */
        unsigned int sync_interval;
};

/**
 * @brief Create a new mptcpd configuration.
 *
 * @param[in]     argc Command line argument count.
 * @param[in,out] argv Command line argument vector.
 *
 * @return Mptcpd configuration information.
 */
struct mptcpd_config *mptcpd_config_create(int argc, char *argv[]);

/**
 * @brief Destroy the mptcpd configuration.
 *
 * Deallocate all mptcpd configuration resources.
 *
 * @param[in,out] config Mptcpd configuration to be finalized.
 */
void mptcpd_config_destroy(struct mptcpd_config *config);

#endif  // MPTCPD_CONFIGURATION_H

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
