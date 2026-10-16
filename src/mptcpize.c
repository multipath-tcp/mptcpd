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

#include <argp.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <mptcpd/private/config.h>
#endif

#ifdef HAVE_ERROR_H
# include <error.h>
#endif

#define SYSTEMD_ENV_VAR		"Environment="
#define SYSTEMD_OVERRIDE_FILE	"mptcpize.conf"
#define SYSTEMD_UNIT_VAR	"FragmentPath="
#define SYSTEMD_SERVICE_TAG	"[Service]"
#define SYSTEMCTL_SHOW		"systemctl show -p FragmentPath "
#define PRELOAD_VAR		"LD_PRELOAD="
#define MPTCPWRAP_ENV		PKGLIBDIR"/libmptcpwrap.so.0.0."LIBREVISION
#define GODEBUG_VAR		"GODEBUG="
#define MPTCPGO_ENV		"multipathtcp=1"

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
        "\tenable <unit>             Update the systemd <unit> configuration,\n"
        "\t                          forcing the given service to run under\n"
        "\t                          the above launcher by adding a new file\n"
        "\t                          (mptcpize.conf) in the service drop-in\n"
        "\t                          directory, e.g. foo.service.d.\n\n"
        "\tdisable <unit>            Update the systemd <unit> configuration,\n"
        "\t                          removing the above launcher.\n";

#ifndef HAVE_ERROR_H
# define ERROR_HELPER(status, errnum, format, ...) do {		\
	if (errnum) {						\
		errno = errnum;					\
		perror(format);					\
	} else {						\
		fprintf(stderr, format "%s", __VA_ARGS__);	\
	}							\
	if (status)						\
		exit(status);					\
   } while(0)
# define error(...) ERROR_HELPER(__VA_ARGS__, "\n")
#endif

static struct argp const argp = { 0, 0, args_doc, doc, 0, 0, 0 };

static void help(void)
{
	argp_help(&argp, stderr, ARGP_HELP_STD_HELP, "mptcpize");
}

static int run(int argc, char *av[])
{
	int i, nr = 0, ld, go, debug = 0;
	char **envp, **argv, *env;
	size_t len;

	if (argc > 0 && strcmp(av[0], "-d") == 0) {
		debug = 1;
		argc--;
		av++;
	}

	if (argc < 1) {
		fprintf(stderr, "missing command argument\n");
		help();
		return -1;
	}

	// build environment, copying the current one ...
	while (environ[nr])
		nr++;
	envp = calloc(nr + 4, sizeof(char *));
	if (!envp)
		error(1, errno, "can't allocate env list");

	// ... filtering out any 'LD_PRELOAD' and 'GODEBUG' ...
	nr = 0;
	i = 0;
	ld = -1;
	go = -1;
	while (environ[nr]) {
		if (strncmp(environ[nr], PRELOAD_VAR,
			    strlen(PRELOAD_VAR)) == 0) {
			ld = nr;
		} else if (strncmp(environ[nr], GODEBUG_VAR,
				   strlen(GODEBUG_VAR)) == 0) {
			go = nr;
		} else {
			envp[i] = environ[nr];
			i++;
		}
		nr++;
	}

	// ... appending the mptcpwrap preload...
	if (ld >= 0) {
		len = strlen(environ[ld]) + strlen(MPTCPWRAP_ENV) + 2;
		env = alloca(len);
		snprintf(env, len, "%s:%s", environ[ld], MPTCPWRAP_ENV);
	} else {
		env = PRELOAD_VAR MPTCPWRAP_ENV;
	}
	envp[i++] = env;

	// .. and GODEBUG=multipathtcp=1 ...
	if (go >= 0) {
		len = strlen(environ[go]) + strlen(MPTCPGO_ENV) + 2;
		env = alloca(len);
		snprintf(env, len, "%s,%s", environ[go], MPTCPGO_ENV);
	} else {
		env = GODEBUG_VAR MPTCPGO_ENV;
	}
	envp[i++] = env;

	// ... and enable dbg if needed
	if (debug)
		envp[i++] = "MPTCPWRAP_DEBUG=1";

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
	char dir_path[PATH_MAX], conf_path[PATH_MAX];
	char *unit;
	FILE *conf;

	if (argc < 1) {
		fprintf(stderr, "missing unit argument\n");
		help();
		return -1;
	}

	unit = locate_unit(argv[0]);

	/* systemd drop-in: <unit>.d/mptcpize.conf */
	if (snprintf(dir_path, sizeof(dir_path), "%s.d", unit) >= (int)sizeof(dir_path) ||
	    snprintf(conf_path, sizeof(conf_path), "%s/" SYSTEMD_OVERRIDE_FILE,
		     dir_path) >= (int)sizeof(conf_path))
		error(1, 0, "path too long for unit %s", unit);

	if (enable) {
		if (mkdir(dir_path, 0755) < 0 && errno != EEXIST)
			error(1, errno, "can't create directory %s", dir_path);

		conf = fopen(conf_path, "w");
		if (!conf)
			error(1, errno, "can't open %s for writing", conf_path);

		if (fprintf(conf, "%s\n%s%s\n", SYSTEMD_SERVICE_TAG,
			    SYSTEMD_ENV_VAR, PRELOAD_VAR MPTCPWRAP_ENV) < 0)
			error(1, errno, "can't write to %s", conf_path);
		if (fclose(conf) != 0)
			error(1, errno, "can't write to %s", conf_path);
	} else {
		if (unlink(conf_path) < 0 && errno != ENOENT)
			error(1, errno, "can't remove %s", conf_path);

		/* best effort: only succeeds if the directory is empty */
		rmdir(dir_path);
	}

	if (system("systemctl daemon-reload") != 0)
		error(1, errno, "can't reload unit, manual 'systemctl daemon-reload' is required");

	printf("mptcp successfully %s on unit %s\n",
	       enable ? "enabled" : "disabled", conf_path);
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
