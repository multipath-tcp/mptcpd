// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file configuration.c
 *
 * @brief Mptcpd configuration parser implementation.
 *
 * Copyright (c) 2017-2021, Intel Corporation
 */

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>  // For NDEBUG
#endif

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <argp.h>
#include <ctype.h>  // For isalnum().
#include <assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <ell/log.h>
#include <ell/util.h>
#include <ell/settings.h>

#include <mptcpd/types.h>

#include <mptcpd/private/configuration.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

/**
 * @name Preprocessor Based String Concatenation
 *
 * Preprocessor concatenation that expands preprocessor tokens as
 * needed by leveraging the usual indirection technique.
 */
///@{
/// Underlying string concatenation macro.
#define MPTCPD_CONCAT_IMPL(x, ...) x ## __VA_ARGS__

/// Concatenate strings using the preprocessor.
#define MPTCPD_CONCAT(x, ...) MPTCPD_CONCAT_IMPL(x, __VA_ARGS__)
///@}

// Compile-time default logging choice
#ifndef MPTCPD_LOGGER
// This should never occur!
# error Problem configuring default log message destination.
#endif
/// Name of the default logging function determined at compile-time.
#define MPTCPD_SET_LOG_FUNCTION MPTCPD_CONCAT(l_log_set_, MPTCPD_LOGGER)

/**
 * @brief Get the function that sets the log message destination.
 *
 * Logging in mptcpd is done through ELL. This function returns
 * function that sets the underlying logging mechanism in ELL.
 *
 * @param[in] l The name of the logging mechanism to be used.  It must
 *              have a corresponding ELL @c l_log_set_...() function
 *              (e.g. @c l_log_set_journal).  This argument may be
 *              NULL, in which case the logging mechanism will chosen
 *              through the mptcpd settings file or the compile-time
 *              default chosen through the configure script.
 *
 * @return Function that sets the underlying logging mechanism in
 *         ELL.
 */
static mptcpd_set_log_func_t get_log_set_function(char const *l)
{
        mptcpd_set_log_func_t log_set = NULL;

        if (l == NULL)
                return log_set;  // Use compile-time selected logger.

        if (strcmp(l, "stderr") == 0)
                return l_log_set_stderr;
        else if (strcmp(l, "syslog") == 0)
                return l_log_set_syslog;
        else if (strcmp(l, "journal") == 0)
                return l_log_set_journal;
        else if (strcmp(l, "null") == 0)
                log_set = l_log_set_null;

        return log_set;
}

static int append_tok(char *str, int len, const char *sep, const char *tok)
{
        int tok_len = strlen(tok);
        int sep_len = strlen(sep);

        if (len <= tok_len + sep_len)
                return len;

        l_strlcpy(str, sep, len);
        l_strlcpy(str + sep_len, tok, len - sep_len);
        return tok_len + sep_len;
}

struct tok_entry
{
        uint32_t id;
        const char *string;
};

static struct tok_entry addr_flags_toks[] = {
        { MPTCPD_ADDR_FLAG_SUBFLOW, "subflow" },
        { MPTCPD_ADDR_FLAG_SIGNAL, "signal" },
        { MPTCPD_ADDR_FLAG_BACKUP, "backup" },
        { 0, NULL },
};


/**
 * @brief converts the flags into a string representation
 *
 * Given a flags bitmask converts it to string representation
 * using the specified token list and storing it into the
 * provided string buffer.
 *
 * @param[in]     toks mapping from flag to token
 *
 * @param[in]    flags address flags to be converted
 *
 * @param[out]     str place the corresponding string representation
 *                     in this buffer
 * @param[in]      len length of the string buffer @c str
 */
static const char *flags_string(const struct tok_entry *toks,
                                uint32_t flags, char *str, int len)
{
        const struct tok_entry *tok;
        const char *sep="";
        int ret;

        str[0] = 0;
        for (tok = toks; tok->id; tok++) {
               if (flags & tok->id) {
                       ret = append_tok(str, len, sep, tok->string);
                       str += ret;
                       len -= ret;
                       sep = ",";
               }
        }
        return str;
}

static const char *addr_flags_string(uint32_t flags, char *str, int len)
{
        return flags_string(addr_flags_toks, flags, str, len);
}

struct tok_entry notify_flags_toks[] = {
        { MPTCPD_NOTIFY_FLAG_EXISTING, "existing" },
        { MPTCPD_NOTIFY_FLAG_SKIP_LL, "skip_link_local" },
        { MPTCPD_NOTIFY_FLAG_SKIP_HOST, "skip_loopback" },
        { 0, NULL },
};

static const char *notify_flags_string(uint32_t flags, char *str, int len)
{
        return flags_string(notify_flags_toks, flags, str, len);
}


/**
 * @brief converts the addr-flags string into numeric rep
 *
 * String typed fields in the @c mptcpd_config structure contain
 * dynamically allocated string
 *
 * @param[in]   toks mapping from flag to token
 *
 * @param[in]    str the address flags string to be converted
 */
static uint32_t flags_from_string(const struct tok_entry *toks, const char *str)
{
        const struct tok_entry *tok;
        int len = strlen(str);
        uint32_t ret = 0;

        while (len > 0) {
                for (tok = toks; tok->id; tok++) {
                      int tok_len = strlen(tok->string);

                      if (strncmp(str, tok->string, tok_len) ||
                          (str[tok_len] != 0 && str[tok_len] != ','))
                              continue;

                      ret |= tok->id;
                      if (str[tok_len] == ',')
                              tok_len++;
                      len -= tok_len;
                      str += tok_len;
                      break;
                }

                if (!tok->id) {
                      l_warn("unknown flag %s", str);
                      return ret;
                }
        }
        return ret;
}

static uint32_t addr_flags_from_string(const char *str)
{
        return flags_from_string(addr_flags_toks, str);
}

static uint32_t notify_flags_from_string(const char *str)
{
        return flags_from_string(notify_flags_toks, str);
}


// ---------------------------------------------------------------
// Set configuration values
// ---------------------------------------------------------------
/**
 * @brief Reset @c mptcpd_config string typed field.
 *
 * String typed fields in the @c mptcpd_config structure contain
 * dynamically allocated strings.  Deallocate such strings before
 * resetting their value.
 *
 * @param[in,out] dest Pointer to memory containing string to be
 *                     reassigned.
 * @param[in]     src  Dynamically allocated string to assigned
 *                     @c *dest.
 */
static void reset_string(char const **dest, char const *src)
{
        assert(dest != NULL);  // *dest may be NULL.

        l_free((char *) *dest);
        *dest = src;
}

/**
 * @brief Set mptcpd plugin directory.
 *
 * Set mptcpd plugin directory in the mptcpd configuration, @a config,
 * being careful to deallocate a previously set plugin directory.
 *
 * @param[in,out] config Mptcpd configuration.
 * @param[in]     dir    Mptcpd plugin directory.  Ownership of memory
 *                       is transferred to @a config.
 */
static void set_plugin_dir(struct mptcpd_config *config, char const *dir)
{
        reset_string(&config->plugin_dir, dir);
}

/**
 * @brief Set default mptcpd plugin name.
 *
 * Set the default mptcpd plugin name in the mptcpd configuration,
 * @a config, being careful to deallocate a previously set default
 * plugin name.
 *
 * @param[in,out] config Mptcpd configuration.
 * @param[in]     plugin Default mptcpd plugin name.  Ownership of
 *                       memory is transferred to @a config.
 */
static void set_default_plugin(struct mptcpd_config *config,
                               char const *plugin)
{
        reset_string(&config->default_plugin, plugin);
}

// ---------------------------------------------------------------
// Command line options
// ---------------------------------------------------------------
static char const doc[] = "Start the Multipath TCP daemon.";

/**
 * @name Command Line Option Key Values
 *
 * Non-ASCII key values for options without a short option (e.g. -d).
 */
///@{
/// Command line option key for "--plugin-dir".
#define MPTCPD_PLUGIN_DIR_KEY 0x100

/// Command line option key for "--path-manager".
#define MPTCPD_PATH_MANAGER_KEY 0x101

/// Command line option key for "--addr-flags" 
#define MPTCPD_ADDR_FLAGS_KEY 0x102

/// Command line option key for "--notify-flags"
#define MPTCPD_NOTIFY_FLAGS_KEY 0x103
///@}

static struct argp_option const options[] = {
        { "debug", 'd', 0, 0, "Enable debug log messages", 0 },
        { "log",
          'l',
          "DEST",
          0,
          "Log to DEST (stderr, syslog or journal), e.g. --log=journal",
          0 },
        { "notify-flags",
          MPTCPD_NOTIFY_FLAGS_KEY,
          "FLAGS",
          0,
          "FLags for address notify notification, e.g. --notify-flags=existing,skip_link_local,skip_loopback",
          0 },
        { "plugin-dir",
          MPTCPD_PLUGIN_DIR_KEY,
          "DIR",
          0,
          "Set plugin directory to DIR",
          0 },
        { "path-manager",
          MPTCPD_PATH_MANAGER_KEY,
          "PLUGIN",
          0,
          "Set default path manager to PLUGIN, e.g. --path-manager=sspi, "
          "overriding plugin priorities",
          0 },
        { "addr-flags",
          MPTCPD_ADDR_FLAGS_KEY,
          "FLAGS",
          0,
          "Set flags for announced address, e.g. --addr-flags=subflow",
          0 },
        { 0 }
};

/// argp parser function.
static error_t parse_opt(int key, char *arg, struct argp_state *state)
{
        struct mptcpd_config *const config = state->input;

        switch (key) {
        case 'd':
                l_debug_enable("*");
                break;
        case 'l':
                config->log_set = get_log_set_function(arg);
                if (config->log_set == NULL)
                        argp_error(state,
                                   "Unknown logging option: \"%s\"",
                                   arg);

                break;
        case MPTCPD_PLUGIN_DIR_KEY:
                if (strlen(arg) == 0)
                        argp_error(state,
                                   "Empty plugin directory command "
                                   "line option.");

                set_plugin_dir(config, l_strdup(arg));
                break;
        case MPTCPD_PATH_MANAGER_KEY:
                if (strlen(arg) == 0)
                        argp_error(state,
                                   "Empty default path manager "
                                   "plugin command line option.");

                set_default_plugin(config, l_strdup(arg));
                break;
        case MPTCPD_ADDR_FLAGS_KEY:
                config->addr_flags = addr_flags_from_string(arg);
                break;
        case MPTCPD_NOTIFY_FLAGS_KEY:
                config->notify_flags = notify_flags_from_string(arg);
                break;
        default:
                return ARGP_ERR_UNKNOWN;
        };

        return 0;
}

/// mptcpd argp parser configuration.
static struct argp const argp = { options, parse_opt, 0, doc, 0, 0, 0 };

/**
 * @brief Parse command line arguments.

 * @param[in,out] config Mptcpd configuration.
 * @param[in]     argc   Command line argument count.
 * @param[in]     argv   Command line argument vector.
 *
 * @return @c true on successful command line option parse, and
 *         @c false otherwise.
 */
static bool
parse_options(struct mptcpd_config *config, int argc, char *argv[])
{
        assert(config != NULL);

        argp_program_version     = PACKAGE_STRING;
        argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";

        return argp_parse(&argp, argc, argv, 0, NULL, config) == 0;
}

// ---------------------------------------------------------------
// Configuration files
// ---------------------------------------------------------------

/**
 * @brief Verify file permissions are secure.
 *
 * Mptcpd requires that its files are only writable by the owner and
 * group.  Verify that the "other" write mode, @c S_IWOTH, isn't set.
 *
 * @param[in] f Name of file to check for expected permissions.
 *
 * @note There is a TOCTOU race condition between this file
 *       permissions check and subsequent calls to functions that
 *       access the given file @a f, such as the call to
 *       @c l_settings_load_from_file().  There is currently no way
 *       to avoid that with the existing ELL API.
 */
static bool check_file_perms(char const *f)
{
        struct stat sb;
        bool perms_ok = false;

        if (stat(f, &sb) == 0) {
                perms_ok = S_ISREG(sb.st_mode)
                           && (sb.st_mode & S_IWOTH) == 0;

                if (!perms_ok)
                        l_error("\"%s\" should be a file that is not "
                                "world writable.",
                                f);
        } else if (errno == ENOENT) {
                perms_ok = true;

                l_debug("File \"%s\" does not exist.", f);
        } else {
                l_debug("Unexpected error during file "
                        "permissions check.");
        }

        return perms_ok;
}

static void parse_config_log(struct mptcpd_config *config,
                             struct l_settings const *settings,
                             char const *group)
{
        if (config->log_set != NULL)
                return;  // Previously set, e.g. via command line.

        char *const log =
                l_settings_get_string(settings, group, "log");

        if (log != NULL) {
                config->log_set = get_log_set_function(log);

                l_free(log);
        }
}

static void parse_config_plugin_dir(struct mptcpd_config *config,
                                    struct l_settings const *settings,
                                    char const *group)
{
        if (config->plugin_dir != NULL)
                return;  // Previously set, e.g. via command line.

        char *const plugin_dir =
                l_settings_get_string(settings,
                                      group,
                                      "plugin-dir");

        if (plugin_dir != NULL)
                set_plugin_dir(config, plugin_dir);
}

static void parse_config_addr_flags(struct mptcpd_config *config,
                                    struct l_settings const *settings,
                                    char const *group)
{
        if (config->addr_flags != 0)
                return;  // Previously set, e.g. via command line.

        char *const addr_flags =
                l_settings_get_string(settings,
                                      group,
                                      "addr-flags");

        if (addr_flags != NULL)
                config->addr_flags = addr_flags_from_string(addr_flags);
}

static void parse_config_notify_flags(struct mptcpd_config *config,
                                      struct l_settings const *settings,
                                      char const *group)
{
        if (config->notify_flags != 0)
                return;  // Previously set, e.g. via command line.

        char *const notify_flags =
                l_settings_get_string(settings,
                                      group,
                                      "notify-flags");

        if (notify_flags != NULL)
                config->notify_flags = notify_flags_from_string(notify_flags);
}

static void parse_config_default_plugin(struct mptcpd_config *config,
                                        struct l_settings const *settings,
                                        char const *group)
{
        if (config->default_plugin != NULL)
                return;  // Previously set, e.g. via command line.

        // Default plugin name.
        char *const default_plugin =
                l_settings_get_string(settings,
                                      group,
                                      "path-manager");

        if (default_plugin != NULL)
                set_default_plugin(config, default_plugin);
}

/**
 * @brief Parse configuration file.
 *
 * @param[in,out] config   Mptcpd configuration.
 * @param[in]     filename Configuration file name.
 *
 * @return @c true on successful configuration file parse, and
 *         @c false otherwise.
 */
static bool parse_config_file(struct mptcpd_config *config,
                              char const *filename)
{
        assert(filename != NULL);

        if (!check_file_perms(filename))
                return false;

        struct l_settings *const settings = l_settings_new();
        if (settings == NULL) {
                l_error("Unable to create mptcpd settings.");

                return false;
        }

        if (l_settings_load_from_file(settings, filename)) {
                static char const group[] = "core";

                // Logging mechanism.
                parse_config_log(config, settings, group);

                // Plugin directory.
                parse_config_plugin_dir(config, settings, group);

                // Notification for existing addresses.
                parse_config_notify_flags(config, settings, group);

                // Address flags.
                parse_config_addr_flags(config, settings, group);

                // Default plugin.
                parse_config_default_plugin(config, settings, group);
        } else {
                l_debug("Unable to load mptcpd settings from file '%s'",
                        filename);
        }

        l_settings_free(settings);

        return true;
}

/**
 * @brief Parse configuration files.
 *
 * Parse known configuration files, such as the mptcpd system
 * configuration, those defined by the XDG base directory
 * specification, etc.
 *
 * @param[in,out] config Mptcpd configuration.
 *
 * @return @c true on successful configuration file parse, and
 *         @c false otherwise.
 */
static bool parse_config_files(struct mptcpd_config *config)
{
        assert(config != NULL);

        bool parsed = true;

        /**
         * @todo Should we check for configuration files in
         *       @c ~/.config, @c $XDG_CONFIG_DIRS and
         *       @c $XDG_CONFIG_HOME?  Considering that mptcpd is
         *       meant to run as a privileged process that may not be
         *       a good idea.
         */
        char const *const files[] = { MPTCPD_CONFIG_FILE };

        char const *const *const end = files + L_ARRAY_SIZE(files);
        for (char const *const *file = files;
             file != end && parsed;
             ++file) {
                parsed = parse_config_file(config, *file);
        }

        return parsed;
}

/**
 * @brief Merge mptcpd configurations.
 *
 * Merge mptcpd configuration from @a src to @a dst.
 *
 * @param[in,out] dst Destination mptcpd configuration.
 * @param[in]     src Mptcpd system configuration file.
 *
 * @return @c true on successful merge and @c false otherwise.
 */
static bool merge_config(struct mptcpd_config       *dst,
                         struct mptcpd_config const *src)
{
        if (dst->log_set == NULL)
                dst->log_set = src->log_set;

        if (dst->plugin_dir == NULL)
                dst->plugin_dir = l_strdup(src->plugin_dir);

        if (dst->addr_flags == 0)
                dst->addr_flags = src->addr_flags;

        if (dst->notify_flags == 0)
                dst->notify_flags = src->notify_flags;

        if (dst->default_plugin == NULL)
                dst->default_plugin = l_strdup(src->default_plugin);

        return true;
}

/**
 * @brief Verify that the mptcpd configuration is valid.
 *
 * @param[in] config Mptcpd configuration.
 *
 * @return @c true if the mptcpd configuration parameters are valid
 *         and @c false otherwise.
 */
static bool check_config(struct mptcpd_config const *config)
{
        if (config->plugin_dir == NULL) {
                l_error("mptcpd plugin directory was not configured.");

                return false;
        }

        return true;
}

// ---------------------------------------------------------------
// Public Mptcpd Configuration API
// ---------------------------------------------------------------

struct mptcpd_config *mptcpd_config_create(int argc, char *argv[])
{
        MPTCPD_SET_LOG_FUNCTION();  // For early logging.

        struct mptcpd_config *const config =
                l_new(struct mptcpd_config, 1);

        // No need to check for NULL.  l_new() abort()s on failure.

        // System configuration, e.g. /etc/mptcpd/mptcpd.conf.
        struct mptcpd_config sys_config = { .log_set = NULL };
        static struct mptcpd_config const def_config = {
                .plugin_dir = MPTCPD_DEFAULT_PLUGINDIR };
        char flags[128];

        /*
          Configuration priority:
                  1) Command line
                  2) System config
                  3) Compile-time defaults
         */
        bool const parsed =
                parse_options(config, argc, argv)
                && parse_config_files(&sys_config)
                && merge_config(config, &sys_config)
                && merge_config(config, &def_config)
                && check_config(config);

        l_free((char *) sys_config.default_plugin);
        l_free((char *) sys_config.plugin_dir);

        if (!parsed) {
                // Failed to parse configuration.
                mptcpd_config_destroy(config);

                return NULL;
        }

        // Inform ELL which logger we're going to use.
        if (config->log_set != NULL)
                config->log_set();

        l_debug("path manager plugin directory: %s",
                config->plugin_dir);

        if (config->default_plugin != NULL)
                l_debug("default path manager plugin: %s",
                        config->default_plugin);

        if (config->addr_flags)
                l_debug("address flags: %s",
                        addr_flags_string(config->addr_flags, flags, sizeof(flags)));

        if (config->notify_flags)
                l_debug("notify flags: %s",
                        notify_flags_string(config->notify_flags, flags, sizeof(flags)));

        return config;
}

void mptcpd_config_destroy(struct mptcpd_config *config)
{
        if (config == NULL)
                return;

        l_free((char *) config->default_plugin);
        l_free((char *) config->plugin_dir);
        l_free(config);
}


/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
