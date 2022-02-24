#include <stdlib.h>
#include <stdlib.h>
#include <stdbool.h>
#include <assert.h>
#include <sys/stat.h>
#include <errno.h>

#include <ell/log.h>
#include <ell/util.h>
#include <ell/settings.h>

#include <mptcpd/types.h>

#include <mptcpd/private/configuration.h>

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

bool mptcpd_config_read(char const *filename,
                        mptcpd_parse_func_t fun,
                        void *user_data)
{
        assert(filename != NULL);
        assert(fun != NULL);

        if (!check_file_perms(filename))
                return false;

        struct l_settings *const settings = l_settings_new();
        if (settings == NULL) {
                l_error("Unable to create mptcpd settings.");

                return false;
        }

        if (l_settings_load_from_file(settings, filename))
                fun(settings, user_data);
        else
                l_debug("Unable to load mptcpd settings from file '%s'",
                        filename);

        l_settings_free(settings);

        return true;
}

