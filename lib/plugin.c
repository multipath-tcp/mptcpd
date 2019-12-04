// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin.c
 *
 * @brief Common path manager plugin functions.
 *
 * Copyright (c) 2018, 2019, Intel Corporation
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/mptcp.h>

#include <ell/hashmap.h>
#include <ell/plugin.h>
#include <ell/util.h>
#include <ell/log.h>

#ifdef HAVE_CONFIG_H
#include <mptcpd/config-private.h>
#endif

#include <mptcpd/plugin_private.h>
#include <mptcpd/plugin.h>

/**
 * @todo Remove this preprocessor symbol definition once support for
 *       path management strategy names are supported in the new
 *       generic netlink API.
 *
 * @note @c GENL_NAMSIZ is used as the size since the path manager
 *       name attribute in the deprecated MPTCP generic netlink API
 *       contained a fixed length string of that size.
 */
#ifndef MPTCP_PM_NAME_LEN
# include <linux/genetlink.h>  // For GENL_NAMSIZ
# define MPTCP_PM_NAME_LEN GENL_NAMSIZ
#endif

// ----------------------------------------------------------------
//                         Global variables
// ----------------------------------------------------------------

/**
 * @brief Map of path manager plugins.
 *
 * A map of path manager plugins where the key is the plugin
 * name and the value is a pointer to a @c struct @c mptcpd_pm_ops
 * instance.
 *
 * @note This is a static variable since ELL's plugin framework
 *       doesn't provide a way to pass user data to loaded plugins.
 *       Access to this variable may need to be synchronized if mptcpd
 *       ever supports multiple threads.
 */
static struct l_hashmap *_pm_plugins;

/**
 * @brief Connection token to path manager plugin operations map.
 *
 * @todo Determine if use of a hashmap scales well, in terms
 *       of both performance and resource usage, in the
 *       presence of a large number of MPTCP connections.
 *
 * @note This is a static variable since ELL's plugin framework
 *       doesn't provide a way to pass user data to loaded plugins.
 *       Access to this variable may need to be synchronized if mptcpd
 *       ever supports multiple threads.
 */
static struct l_hashmap *_token_to_ops;

/**
 * @brief Name of default plugin.
 *
 * The corresponding plugin operations will be used by default if no
 * path management strategy was specified for a given MPTCP
 * connection.
 */
static char _default_name[MPTCP_PM_NAME_LEN + 1];

/**
 * @brief Default path manager plugin operations.
 *
 * The operations provided by the path manager plugin with the most
 * favorable (lowest) priority will be used as the default for the
 * case where no specific path management strategy was chosen, or if
 * the chosen strategy doesn't exist.
 */
static struct mptcpd_plugin_ops const *_default_ops;

// ----------------------------------------------------------------
//                      Implementation details
// ----------------------------------------------------------------

/**
 * @brief Verify directory permissions are secure.
 *
 * Mptcpd requires that its directories are only writable by the owner
 * and group.  Verify that the "other" write mode bit, @c S_IWOTH,
 * isn't set.
 *
 * @param[in] dir Name of directory to check for expected
 *                permissions.
 *
 * @note There is a TOCTOU race condition between this directory
 *       permissions check and subsequent calls to functions that
 *       access the given directory @a dir, such as the call to
 *       @c l_plugin_load().  There is currently no way to avoid that
 *       with the existing ELL API.
 */
static bool check_directory_perms(char const *dir)
{
        struct stat sb;

        bool const perms_ok =
                stat(dir, &sb) == 0
                && S_ISDIR(sb.st_mode)
                && (sb.st_mode & S_IWOTH) == 0;

        if (!perms_ok)
                l_error("\"%s\" should be a directory that is not "
                        "world writable.",
                        dir);

        return perms_ok;
}

static struct mptcpd_plugin_ops const *name_to_ops(char const *name)
{
        struct mptcpd_plugin_ops const *ops = _default_ops;

        if (name != NULL) {
                ops = l_hashmap_lookup(_pm_plugins, name);

                if (ops == NULL) {
                        ops = _default_ops;

                        l_error("Requested path management strategy "
                                "\"%s\" does not exist.",
                                name);
                        l_error("Falling back on default.");
                }

        }

        return ops;
}

static struct mptcpd_plugin_ops const *token_to_ops(mptcpd_token_t token)
{
        /**
         * @todo Should we reject a zero valued token?
         */
        struct mptcpd_plugin_ops const *const ops =
                l_hashmap_lookup(_token_to_ops,
                                 L_UINT_TO_PTR(token));

        if (unlikely(ops == NULL))
                l_error("Unable to match token to plugin.");

        return ops;
}

// ----------------------------------------------------------------
//                Plugin Registration and Management
// ----------------------------------------------------------------

bool mptcpd_plugin_load(char const *dir, char const *default_name)
{
        if (dir == NULL) {
                l_error("No plugin directory specified.");
                return false;
        }

        // Plugin directory permissions sanity check.
        if (!check_directory_perms(dir))
                return false;

        if (_pm_plugins == NULL) {
                _pm_plugins = l_hashmap_string_new();

                if (default_name != NULL) {
                        size_t const len = L_ARRAY_SIZE(_default_name);

                        size_t const src_len =
                                l_strlcpy(_default_name,
                                          default_name,
                                          len);

                        if (src_len > len)
                                l_warn("Default plugin name length truncated "
                                       "from %zu to %zu.",
                                       src_len,
                                       len);
                }

                /*
                  No need to check for NULL since
                  l_hashmap_string_new() abort()s on memory allocation
                  failure.
                */

                char *const pattern = l_strdup_printf("%s/*.so", dir);

                // l_strdup_printf() abort()s on failure.

                // All mptcpd plugins are expected to use this symbol
                // in their L_PLUGIN_DEFINE() macro call.
                static char const symbol[] =
                        L_STRINGIFY(MPTCPD_PLUGIN_DESC);

                l_plugin_load(pattern, symbol, VERSION);

                l_free(pattern);

                if (l_hashmap_isempty(_pm_plugins)) {
                        l_hashmap_destroy(_pm_plugins, NULL);
                        _pm_plugins = NULL;

                        return false;  // Plugin load and registration
                                       // failed.
                }

                /**
                 * Create map of connection token to path manager
                 * plugin.
                 *
                 * @note We use the default ELL direct hash function that
                 *       converts from a pointer to an @c unsigned @c int.
                 *
                 * @todo Determine if this is a performance bottleneck on 64
                 *       bit platforms since it is possible that connection
                 *       IDs may end up in the same hash bucket due to the
                 *       truncation from 64 bits to 32 bits in ELL's direct
                 *       hash function, assuming @c unsigned @c int is a 32
                 *       bit type.
                 */
                _token_to_ops = l_hashmap_new();  // Aborts on memory
                                                  // allocation
                                                  // failure.
        }

        return !l_hashmap_isempty(_pm_plugins);
}

void mptcpd_plugin_unload(void)
{
        /**
         * @note This isn't thread-safe.  We'll need a lock if we ever
         *       support destroying multiple path managers from
         *       different threads.  However, right now there doesn't
         *       appear to be a need to support that.
         */
        l_hashmap_destroy(_token_to_ops, NULL);
        l_hashmap_destroy(_pm_plugins, NULL);

        _token_to_ops  = NULL;
        _pm_plugins  = NULL;
        _default_ops = NULL;
        memset(_default_name, 0, sizeof(_default_name));

        l_plugin_unload();
}

bool mptcpd_plugin_register_ops(char const *name,
                                struct mptcpd_plugin_ops const *ops)
{
        if (name == NULL || ops == NULL)
                return false;

        bool const first_registration = l_hashmap_isempty(_pm_plugins);

        bool const registered =
                l_hashmap_insert(_pm_plugins,
                                 name,
                                 (struct mptcpd_plugin_ops *) ops);

        if (registered) {
                /*
                  Set the default plugin operations.

                  If the plugin name matches the default plugin name
                  (if provided) use the corresponding ops.  Otherwise
                  fallback on the first set of registered ops,
                  i.e. those corresponding to a plugin with the most
                  favorable (lowest) priority.
                */
                if (strcmp(_default_name, name) == 0)
                        _default_ops = ops;
                else if (first_registration)
                        _default_ops = ops;
        }

        return registered;
}

// ----------------------------------------------------------------
//               Plugin Operation Callback Invocation
// ----------------------------------------------------------------

void mptcpd_plugin_new_connection(char const *name,
                                  mptcpd_token_t token,
                                  struct sockaddr const *laddr,
                                  struct sockaddr const *raddr,
                                  struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = name_to_ops(name);

        // Map connection token to the path manager plugin operations.
        if (!l_hashmap_insert(_token_to_ops,
                              L_UINT_TO_PTR(token),
                              (void *) ops))
                l_error("Unable to map connection to plugin.");

        ops->new_connection(token, laddr, raddr, pm);
}

void mptcpd_plugin_connection_established(mptcpd_token_t token,
                                          struct sockaddr const *laddr,
                                          struct sockaddr const *raddr,
                                          struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->connection_established(token, laddr, raddr, pm);
}

void mptcpd_plugin_connection_closed(mptcpd_token_t token,
                                     struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->connection_closed(token, pm);
}

void mptcpd_plugin_new_address(mptcpd_token_t token,
                               mptcpd_aid_t id,
                               struct sockaddr const *addr,
                               struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->new_address(token, id, addr, pm);
}

void mptcpd_plugin_address_removed(mptcpd_token_t token,
                                   mptcpd_aid_t id,
                                   struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->address_removed(token, id, pm);
}

void mptcpd_plugin_new_subflow(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               bool backup,
                               struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->new_subflow(token, laddr, raddr, backup, pm);
}

void mptcpd_plugin_subflow_closed(mptcpd_token_t token,
                                  struct sockaddr const *laddr,
                                  struct sockaddr const *raddr,
                                  bool backup,
                                  struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->subflow_closed(token, laddr, raddr, backup, pm);

}

void mptcpd_plugin_subflow_priority(mptcpd_token_t token,
                                    struct sockaddr const *laddr,
                                    struct sockaddr const *raddr,
                                    bool backup,
                                    struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops)
                ops->subflow_priority(token, laddr, raddr, backup, pm);

}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
