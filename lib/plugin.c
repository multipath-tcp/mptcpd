// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file plugin.c
 *
 * @brief Common path manager plugin functions.
 *
 * Copyright (c) 2018-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <unistd.h>
#include <assert.h>

#include <ell/queue.h>
#include <ell/hashmap.h>
#include <ell/util.h>
#include <ell/log.h>

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

#include <mptcpd/private/configuration.h>
#include <mptcpd/private/plugin.h>
#include <mptcpd/plugin.h>


// ----------------------------------------------------------------
//                         Global variables
// ----------------------------------------------------------------

/**
 * @brief Map of path manager plugins.
 *
 * A map of path manager plugins where the key is the plugin name and
 * the value is a pointer to a @c struct @c mptcpd_plugin_ops
 * instance.
 *
 * @todo Consider merging this map the @c plugin_infos queue.
 */
static struct l_hashmap *_pm_plugins;

/**
 * @brief Connection token to path manager plugin operations map.
 *
 * @todo Determine if use of a hashmap scales well, in terms
 *       of both performance and resource usage, in the
 *       presence of a large number of MPTCP connections.
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

static char *_conf_dir;

// ----------------------------------------------------------------
//                      Implementation Details
// ----------------------------------------------------------------

/**
 * @brief Verify directory permissions are secure.
 *
 * Mptcpd requires that its directories are only writable by the owner
 * and group.  Verify that the "other" write mode bit, @c S_IWOTH,
 * isn't set.
 *
 * @param[in] dir Name of plugin directory to check for expected
 *                permissions.
 * @param[in] fd  File descriptor of opened plugin directory to check
 *                for expected permissions.
 */
static bool check_directory_perms(char const *dir,
                                  int fd)
{
        struct stat sb;

        bool const perms_ok =
                fstat(fd, &sb) == 0
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

        if (ops == NULL)
                l_error("Unable to match token to plugin.");

        return ops;
}

// ----------------------------------------------------------------
//                Plugin Registration and Management
// ----------------------------------------------------------------

/**
 * @struct plugin_info
 *
 * @brief Plugin information
 */
struct plugin_info
{
        /// Handle returned from call to @c dlopen().
        void *handle;

        /// Plugin descriptor.
        struct mptcpd_plugin_desc const *desc;
};

/// List of @c plugin_info objects.
static struct l_queue *_plugin_infos;

/**
 * @brief Compare plugin priorities.
 *
 * @param[in] a         New      plugin information.
 * @param[in] b         Existing plugin information.
 * @param[in] user_data User data (unused).
 *
 * @return -1 if the plugin should inserted before the existing
 *         plugin, or 1 otherwise.
 *
 * @see @c l_queue_insert()
 */
static int compare_plugin_priority(void const *a,
                                   void const *b,
                                   void *user_data)
{
        (void) user_data;

        struct plugin_info const *const new      = a;
        struct plugin_info const *const existing = b;

        /*
          Cause "new" plugin to be inserted into the plugin list
          before the "existing" plugin if its priority is higher
          (numerically less) than the existing plugin priority.
        */
        return new->desc->priority < existing->desc->priority ? -1 : 1;
}

static void report_error(int error, char const *msg)
{
        char errmsg[80];
        int const r =
                strerror_r(error, errmsg, L_ARRAY_SIZE(errmsg));

        l_error("%s: %s", msg, r == 0 ? errmsg : "<unknown error>");
}

static void init_plugin(void *data, void *user_data)
{
        struct plugin_info const *const p  = data;
        struct mptcpd_pm         *const pm = user_data;

        if (p->desc->init && p->desc->init(pm) != 0)
                l_warn("Plugin \"%s\" failed to initialize",
                       p->desc->name);
}

static void load_plugin(char const *filename)
{
        void *const handle = dlopen(filename, RTLD_NOW);

        if (handle == NULL) {
                l_error("%s", dlerror());
                return;
        }

        void *const sym = dlsym(handle, L_STRINGIFY(MPTCPD_PLUGIN_SYM));

        if (sym == NULL) {
                l_error("%s", dlerror());
                dlclose(handle);
                return;
        }

        struct mptcpd_plugin_desc const *const desc = sym;

        /**
         * @note We assume that plugin names stored in the plugin
         *       descriptor will be unique.  Path management events
         *       could potentially be dispatched to the wrong plugin
         *       with the same name, otherwise.
         */
        // Require a plugin name since we map it plugin operations.
        if (desc->name == NULL) {
                l_error("No plugin name specified in %s", filename);
                dlclose(handle);
                return;
        }

        struct plugin_info *const p = l_new(struct plugin_info, 1);
        p->handle = handle;
        p->desc   = desc;

        // Register plugin.
        if (!l_queue_insert(_plugin_infos,
                            p,
                            compare_plugin_priority,
                            NULL)) {
                /*
                  We should never get here.  The only way to get here
                  is if either the l_queue pointer argument,
                  i.e. _plugin_infos, is NULL or if the comparison
                  function argument is NULL.
                */
                l_error("Unexpected error registering plugin \"%s\"",
                        filename);
                l_free(p);
                dlclose(handle);
        }

        /*
          Initialization will be performed after all plugins are
          loaded to taken into account plugin priority.
        */
}

static void load_plugins_queue(char const *dir,
                               struct l_queue const* plugins_to_load)
{
        struct l_queue_entry const *entry = 
                l_queue_get_entries((struct l_queue *) plugins_to_load);

        while (entry) {
                char const *const plugin_name = (char *) entry->data;
                char *const path = l_strdup_printf("%s/%s.so",
                                                   dir,
                                                   plugin_name);

                /*
                   No need to verify if the file exists or if it's
                   readable since the dlopen() call in load_plugin()
                   returns NULL if it's not the case. Additional
                   verification, using for example access(), would only
                   introduce a TOCTOU race condition.
                 */
                load_plugin(path);

                l_free(path);

                entry = entry->next;
        }
}

static int load_plugins_all(int const fd,
                            char const *dir)
{
        DIR *const ds = fdopendir(fd);

        if (ds == NULL) {
                report_error(errno,
                             "fdopendir() on plugin directory failed");

                return -1;
        }

        errno = 0;
        for (struct dirent const *d = readdir(ds);
             d != NULL;
             d = readdir(ds)) {
                if ((d->d_type == DT_REG || d->d_type == DT_UNKNOWN)
                    && l_str_has_suffix(d->d_name, ".so")) {
                        char *const path = l_strdup_printf("%s/%s",
                                                           dir,
                                                           d->d_name);

                        load_plugin(path);
                        l_free(path);
                }

                // Reset to detect error on NULL readdir().
                errno = 0;
        }

        int const error = errno;

        (void) closedir(ds);

        /**
         * @todo Should a readdir() error from above be considered
         *       fatal?
         */
        if (error != 0)
                report_error(error, "Error during plugin directory read");

        return error;
}


static int load_plugins(char const *dir, 
                        struct l_queue const *plugins_to_load, 
                        struct mptcpd_pm *pm)
{
        /**
         * @note It would be simpler to implement this function in
         *       terms of @c glob() but that introduces a TOCTOU race
         *       condition between the @c glob() call and subsequent
         *       calls to @c dlopen().  The below approach closes to
         *       the TOCTOU race condition.
         */

        int const fd = open(dir, O_RDONLY | O_DIRECTORY);

        if (fd == -1) {
                report_error(errno, "Unable to open plugin directory");

                return -1;
        }

        // Plugin directory permissions sanity check.
        if (!check_directory_perms(dir, fd)) {
                (void) close(fd);
                return -1;
        }

        int ret = 0;

        if (plugins_to_load) {
                load_plugins_queue(dir, plugins_to_load);
                (void) close(fd);
        } else {
                ret = load_plugins_all(fd, dir);
                /*
                  No need call close() since the fdopendir() call in
                  load_plugins_all() claims ownership of the file
                  descriptor.  There is no ref count bump.
                */
        }

        // Initialize all loaded plugins.
        l_queue_foreach(_plugin_infos, init_plugin, pm);

        return ret;  // 0 on success
}

static bool unload_plugin(void *data, void *user_data)
{
        struct plugin_info *const p  = data;
        struct mptcpd_pm   *const pm = user_data;

        if (p->desc->exit)
                p->desc->exit(pm);

        dlclose(p->handle);
        l_free(p);

        return true;
}

static void unload_plugins(struct mptcpd_pm *pm)
{
        /*
          Unload plugins in the reverse order in which they were
          loaded.
        */
        if (!l_queue_reverse(_plugin_infos))
                return;  // Empty queue

        (void) l_queue_foreach_remove(_plugin_infos, unload_plugin, pm);
        l_queue_destroy(_plugin_infos, NULL);
        _plugin_infos = NULL;
}

bool mptcpd_plugin_load(char const *dir,
                        char const *default_name,
                        char const *plugins_conf_dir,
                        struct l_queue const *plugins_to_load,
                        struct mptcpd_pm *pm)
{
        if (dir == NULL) {
                l_error("No plugin directory specified.");
                return false;
        }

        if (plugins_conf_dir == NULL) {
                l_error("No plugins configuration directory specified.");
                return false;
        }

        if (_conf_dir == NULL)
                _conf_dir = l_strdup(plugins_conf_dir);

        if (_plugin_infos == NULL)
                _plugin_infos = l_queue_new();

        if (_pm_plugins == NULL) {
                _pm_plugins = l_hashmap_string_new();

                /*
                  No need to check for NULL since
                  l_hashmap_string_new() abort()s on memory allocation
                  failure.
                */

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

                if (load_plugins(dir, plugins_to_load, pm) != 0
                    || l_hashmap_isempty(_pm_plugins)) {
                        l_hashmap_destroy(_pm_plugins, NULL);
                        _pm_plugins = NULL;
                        unload_plugins(pm);

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

void mptcpd_plugin_unload(struct mptcpd_pm *pm)
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

        unload_plugins(pm);
}

bool mptcpd_plugin_register_ops(char const *name,
                                struct mptcpd_plugin_ops const *ops)
{
        if (name == NULL || ops == NULL)
                return false;

        /**
         * @todo Should we return @c false if all of the callbacks in
         *       @a ops are @c NULL?
         */
        if (ops->new_connection            == NULL
            && ops->connection_established == NULL
            && ops->connection_closed      == NULL
            && ops->new_address            == NULL
            && ops->address_removed        == NULL
            && ops->new_subflow            == NULL
            && ops->subflow_closed         == NULL
            && ops->subflow_priority       == NULL
            && ops->new_interface          == NULL
            && ops->update_interface       == NULL
            && ops->delete_interface       == NULL
            && ops->new_local_address      == NULL
            && ops->delete_local_address   == NULL)
                l_warn("No plugin operations were set.");

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

bool mptcpd_plugin_read_config(char const *filename,
                               mptcpd_parse_func_t fun,
                               void *user_data)
{
        assert(filename != NULL);
        assert(fun != NULL);

        char *const path = l_strdup_printf("%s/%s.conf",
                                           _conf_dir,
                                           filename);

        bool success = mptcpd_config_read(path, fun, user_data);

        l_free(path);

        return success;
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

        if (ops && ops->new_connection)
                ops->new_connection(token, laddr, raddr, pm);
}

void mptcpd_plugin_connection_established(mptcpd_token_t token,
                                          struct sockaddr const *laddr,
                                          struct sockaddr const *raddr,
                                          struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->connection_established)
                ops->connection_established(token, laddr, raddr, pm);
}

void mptcpd_plugin_connection_closed(mptcpd_token_t token,
                                     struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->connection_closed)
                ops->connection_closed(token, pm);
}

void mptcpd_plugin_new_address(mptcpd_token_t token,
                               mptcpd_aid_t id,
                               struct sockaddr const *addr,
                               struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->new_address)
                ops->new_address(token, id, addr, pm);
}

void mptcpd_plugin_address_removed(mptcpd_token_t token,
                                   mptcpd_aid_t id,
                                   struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->address_removed)
                ops->address_removed(token, id, pm);
}

void mptcpd_plugin_new_subflow(mptcpd_token_t token,
                               struct sockaddr const *laddr,
                               struct sockaddr const *raddr,
                               bool backup,
                               struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->new_subflow)
                ops->new_subflow(token, laddr, raddr, backup, pm);
}

void mptcpd_plugin_subflow_closed(mptcpd_token_t token,
                                  struct sockaddr const *laddr,
                                  struct sockaddr const *raddr,
                                  bool backup,
                                  struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->subflow_closed)
                ops->subflow_closed(token, laddr, raddr, backup, pm);

}

void mptcpd_plugin_subflow_priority(mptcpd_token_t token,
                                    struct sockaddr const *laddr,
                                    struct sockaddr const *raddr,
                                    bool backup,
                                    struct mptcpd_pm *pm)
{
        struct mptcpd_plugin_ops const *const ops = token_to_ops(token);

        if (ops && ops->subflow_priority)
                ops->subflow_priority(token, laddr, raddr, backup, pm);

}

// ----------------------------------------------------------------
// Network Monitoring Related Plugin Operation Callback Invocation
// ----------------------------------------------------------------

/**
 * @struct plugin_address_info
 *
 * @brief Convenience structure to bundle address information.
 */
struct plugin_address_info
{
        /// Network interface information.
        struct mptcpd_interface const *const interface;

        /// Network address information.
        struct sockaddr const *const address;

        /// Mptcpd path manager object.
        struct mptcpd_pm *const pm;
};

/**
 * @struct plugin_interface_info
 *
 * @brief Convenience structure to bundle interface information.
 */
struct plugin_interface_info
{
        /// Network interface information.
        struct mptcpd_interface const *const interface;

        /// Mptcpd path manager object.
        struct mptcpd_pm *const pm;
};

static void new_interface(void const *key, void *value, void *user_data)
{
        (void) key;

        assert(value != NULL);

        struct mptcpd_plugin_ops     const *const ops = value;
        struct plugin_interface_info const *const i   = user_data;

        if (ops->new_interface)
                ops->new_interface(i->interface, i->pm);
}

static void update_interface(void const *key,
                             void *value,
                             void *user_data)
{
        (void) key;

        assert(value != NULL);

        struct mptcpd_plugin_ops     const *const ops = value;
        struct plugin_interface_info const *const i   = user_data;

        if (ops->update_interface)
                ops->update_interface(i->interface, i->pm);
}

static void delete_interface(void const *key,
                             void *value,
                             void *user_data)
{
        (void) key;

        assert(value != NULL);

        struct mptcpd_plugin_ops     const *const ops = value;
        struct plugin_interface_info const *const i   = user_data;

        if (ops->delete_interface)
                ops->delete_interface(i->interface, i->pm);
}

static void new_local_address(void const *key,
                              void *value,
                              void *user_data)
{
        (void) key;

        assert(value != NULL);

        struct mptcpd_plugin_ops   const *const ops = value;
        struct plugin_address_info const *const i   = user_data;

        if (ops->new_local_address)
                ops->new_local_address(i->interface, i->address, i->pm);
}

static void delete_local_address(void const *key,
                                 void *value,
                                 void *user_data)
{
        (void) key;

        assert(value != NULL);

        struct mptcpd_plugin_ops    const *const ops = value;
        struct plugin_address_info  const *const i   = user_data;

        if (ops->delete_local_address)
                ops->delete_local_address(i->interface, i->address, i->pm);
}

void mptcpd_plugin_new_interface(struct mptcpd_interface const *i,
                                 void *pm)
{
        struct plugin_interface_info info = {
                .interface = i,
                .pm        = pm
        };

        l_hashmap_foreach(_pm_plugins, new_interface, &info);
}

void mptcpd_plugin_update_interface(struct mptcpd_interface const *i,
                                    void *pm)
{
        struct plugin_interface_info info = {
                .interface = i,
                .pm        = pm
        };

        l_hashmap_foreach(_pm_plugins, update_interface, &info);
}

void mptcpd_plugin_delete_interface(struct mptcpd_interface const *i,
                                    void *pm)
{
        struct plugin_interface_info info = {
                .interface = i,
                .pm        = pm
        };

        l_hashmap_foreach(_pm_plugins, delete_interface, &info);
}

void mptcpd_plugin_new_local_address(struct mptcpd_interface const *i,
                                     struct sockaddr const *sa,
                                     void *pm)
{
        struct plugin_address_info info = {
                .interface = i,
                .address   = sa,
                .pm        = pm
        };

        l_hashmap_foreach(_pm_plugins, new_local_address, &info);
}

void mptcpd_plugin_delete_local_address(struct mptcpd_interface const *i,
                                        struct sockaddr const *sa,
                                        void *pm)
{
        struct plugin_address_info info = {
                .interface = i,
                .address   = sa,
                .pm        = pm
        };

        l_hashmap_foreach(_pm_plugins, delete_local_address, &info);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
