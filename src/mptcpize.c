// SPDX-License-Identifier: BSD-3-Clause
/**
 * @file mptcpize.c
 *
 * @brief enable mptcp on existing services.
 *
 * Copyright (c) 2021, Red Hat, Inc.
 */

#define  _GNU_SOURCE

#include <linux/limits.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sendfile.h>

#include <argp.h>
#include <dlfcn.h>
#include <errno.h>
#include <error.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#define SYSTEMD_ENV_VAR		"Environment="
#define SYSTEMD_UNIT_VAR	"FragmentPath="
#define SYSTEMD_SERVICE_TAG	"[Service]"
#define SYSTEMCTL_SHOW		"systemctl show -p FragmentPath "
#define PRELOAD_VAR		"LD_PRELOAD="
#define MPTCPWRAP_ENV		"LD_PRELOAD="PKGLIBDIR"/libmptcpwrap.so.0.0."LIBREVISION

/* Program documentation. */
static char args_doc[] = "CMD";

static char doc[] =
        "mptcpize - a tool to enable MPTCP usage on unmodified legacy services\v"
        "Available CMDs:\n"
        "\trun [-d] prog [<args>]    Run target program with specified\n"
        "\t                          arguments, forcing MPTCP socket usage\n"
        "\t                          instead of TCP.  If the '-d' argument\n"
        "\t                          is provided, dump messages on stderr\n"
        "\t                          when a TCP socket is forced to MPTCP.\n\n"
        "\tenable <unit>             Update the systemd <unit>, forcing\n"
        "\t                          the given service to run under the\n"
        "\t                          above launcher.\n\n"
        "\tdisable <unit>            Update the systemd <unit>, removing\n"
        "\t                          the above launcher.\n";

static struct argp const argp = { 0, 0, args_doc, doc, 0, 0, 0 };

static void help(void)
{
	argp_help(&argp, stderr, ARGP_HELP_STD_HELP, "mptcpize");
}

static int run(int argc, char *av[])
{
	int i, nr = 0, debug = 0, toa = 0;
	char **envp, **argv;
        //TII
        char* tmp;

	if (argc > 0 && strcmp(av[0], "-d") == 0) {
		debug = 1;
		argc--;
		av++;
	}
    
        //TII - Option for type of application
        if (argc > 0 && strcmp(av[0], "-a") == 0) {
		toa = atoi(av[1]);
                argc = argc - 2;
                av = av + 2;
        }

	if (argc < 1) {
		fprintf(stderr, "missing command argument\n");
		help();
		return -1;
	}

	// build environment, copying the current one ...
	while (environ[nr])
		nr++;
	envp = calloc(nr + 3, sizeof(char *));
	if (!envp)
		error(1, errno, "can't allocate env list");

	// ... filtering out any 'LD_PRELOAD' ...
	nr = 0;
	i = 0;
	while (environ[nr]) {
		if (strncmp(environ[nr], PRELOAD_VAR,
			    strlen(PRELOAD_VAR)) != 0) {
			envp[i] = environ[nr];
			i++;
		}
		nr++;
	}

	// ... appending the mptcpwrap preload...
	envp[i++] = MPTCPWRAP_ENV;

	// ... and enable dbg if needed
	if (debug)
		envp[i++] = "MPTCPWRAP_DEBUG=1";

        //TII - Setting the application type as environment variable
        if (toa){
		asprintf(&tmp, "MPTCPWRAP_TOA=%d",toa);
                envp[i++] = tmp;
        }
	// build the NULL terminated arg list
	argv = calloc(argc + 1, sizeof(char *));
	if (!argv)
		error(1, errno, "can't allocate argument list");

	memcpy(argv, av, argc * sizeof(char*));
	return execvpe(argv[0], argv, envp);
}

static char *locate_unit(const char *name)
{
	char *cmd, *line = NULL;
	FILE *systemctl;
	size_t len = 0;
	ssize_t read;

	/* check for existing unit file */
	if (access(name, R_OK) == 0)
		return strdup(name);

	/* this is supposed to be an unit name */
	len = strlen(name) + 1 + strlen(SYSTEMCTL_SHOW);
	cmd = malloc(len);
	if (!cmd)
		error(1, 0, "can't allocate systemctl command string");

	sprintf(cmd, SYSTEMCTL_SHOW"%s", name);
	systemctl = popen(cmd, "r");
	if (!systemctl)
		error(1, errno, "can't execute %s", cmd);

	free(cmd);
	while ((read = getline(&line, &len, systemctl)) != -1) {
		if (strncmp(line, SYSTEMD_UNIT_VAR, strlen(SYSTEMD_UNIT_VAR)) == 0) {
			char *ret = strdup(&line[strlen(SYSTEMD_UNIT_VAR)]);
			if (!ret)
				error(1, errno, "failed to duplicate string");

			// trim trailing newline, if any
			len = strlen(ret);
			if (len > 0 && ret[len - 1] == '\n')
				ret[--len] = 0;
			if (len == 0)
				error(1, 0, "can't find unit file for service %s", name);
			free(line);
			pclose(systemctl);
			return ret;
		}
	}

	error(1, 0, "can't find FragmentPath attribute for unit %s", name);

	// never reached: just silence gcc
	return NULL;
}

static int unit_update(int argc, char *argv[], int enable)
{
	char *unit, *line = NULL;
	int append_env = enable;
	char dst_path[PATH_MAX];
	off_t bytes_copied = 0;
	struct stat fileinfo;
	int dst, unit_fd;
	size_t len = 0;
	ssize_t read;
	FILE *src;

	if (argc < 1) {
		fprintf(stderr, "missing unit argument\n");
		help();
		return -1;
	}

	unit = locate_unit(argv[0]);
	src = fopen(unit, "r");
	if (!src)
		error(1, errno, "can't open file %s", unit);

	strcpy(dst_path, "/tmp/unit_XXXXXX");
	dst = mkstemp(dst_path);
	if (dst < 0)
		error(1, errno, "can't create tmp file");

	// reset any prior error, to allow later check on errno
	errno = 0;

	/**
	 * systemd does not allow Environment property update via a command,
	 * we have to copy the existing unit file to a tmp one
	 */
	while ((read = getline(&line, &len, src)) != -1) {
		int is_env = strncmp(line, SYSTEMD_ENV_VAR, strlen(SYSTEMD_ENV_VAR)) == 0;

		if (!is_env) {
			if (write(dst, line, read) < 0)
				error(1, errno, "can't write '%s' into %s", line, dst_path);
		}

		if (append_env &&
		    (is_env || strncmp(line, SYSTEMD_SERVICE_TAG, strlen(SYSTEMD_SERVICE_TAG)) == 0)) {
			if (dprintf(dst, "%s%s\n", SYSTEMD_ENV_VAR, MPTCPWRAP_ENV) < 0)
				error(1, errno, "can't write to env string into %s", dst_path);
			append_env = 0;
		}
	}
	if (errno != 0)
		error(1, errno, "can't read from %s", unit);
	free(line);
	fclose(src);

	// copy back the modified file into the original unit
	// note: avoid using rename, as it fails across filesystems
	if (fstat(dst, &fileinfo) < 0)
		error(1, errno, "can't stat %s", dst_path);

	// re-open the unit file for writing
	// mkstemp already opened the temporary file for R/W so we don't need
	// to touch that file descriptor.
	unit_fd = open(unit, O_TRUNC | O_RDWR);
	if (unit_fd < 0)
		error(1, errno, "can't open %s for writing", unit);

	while (bytes_copied < fileinfo.st_size)
		if (sendfile(unit_fd, dst, &bytes_copied, fileinfo.st_size - bytes_copied) < 0)
			error(1, errno, "can't copy from %s to %s", dst_path, unit);

	close(dst);
	if (system("systemctl daemon-reload") != 0)
		error(1, errno, "can't reload unit, manual 'systemctl daemon-reload' is required");

	printf("mptcp successfully %s on unit %s\n",
	       enable ? "enabled" : "disabled", unit);
	free(unit);
	return 0;
}

static int enable(int argc, char *argv[])
{
	return unit_update(argc, argv, 1);
}

static int disable(int argc, char *argv[])
{
	return unit_update(argc, argv, 0);
}

int main(int argc, char *argv[])
{
	int idx;

	argp_program_version = "mptcpize "VERSION;
	argp_program_bug_address = "<" PACKAGE_BUGREPORT ">";
	if (argp_parse(&argp, argc, argv, ARGP_IN_ORDER, &idx, 0) < 0)
		error(1, errno, "can't parse arguments");

	argc -= idx;
	argv += idx;
	while (argc > 0) {
		if (strcmp(argv[0], "run") == 0)
			return run(--argc, ++argv);
		else if (strcmp(argv[0], "enable") == 0)
			return enable(--argc, ++argv);
		else if (strcmp(argv[0], "disable") == 0)
			return disable(--argc, ++argv);
		else if (strcmp(argv[0], "help") == 0) {
			help();
			return 0;
		} else {
			fprintf(stderr, "unknown arg %s\n", argv[0]);
			return -1;
		}
	}
	help();
	return 0;
}

/*
  Local Variables:
  c-file-style: "linux"
  End:
*/
