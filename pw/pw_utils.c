/*-
 * Copyright (C) 2015 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/wait.h>

#include <ctype.h>
#include <err.h>
#include <limits.h>
#include <sysexits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pw.h"
#include "pathnames.h"

#define _PATH_YPDIR "/var/yp"
#define _PATH_YP_MAKEFILE _PATH_YPDIR "/Makefile"

int
pw_checkfd(char *nptr)
{
	const char *errstr;
	int fd = -1;

	if (strcmp(nptr, "-") == 0)
		return '-';
	fd = strtonum(nptr, 0, INT_MAX, &errstr);
	if (errstr != NULL)
		errx(EX_USAGE, "Bad file descriptor '%s': %s",
		    nptr, errstr);
	return (fd);
}

uintmax_t
pw_checkid(char *nptr, uintmax_t maxval)
{
	const char *errstr = NULL;
	uintmax_t id;

	id = strtounum(nptr, 0, maxval, &errstr);
	if (errstr)
		errx(EX_USAGE, "Bad id '%s': %s", nptr, errstr);
	return (id);
}

bool
pw_id_numeric(const char *nptr)
{
	const unsigned char *p = (const unsigned char *)nptr;

	if (p == NULL || *p == '\0')
		return (false);
#ifdef __APPLE__
	if (*p == '+' || *p == '-')
		p++;
#elif defined(__FreeBSD__)
	/* FreeBSD CLI IDs are unsigned decimal. */
#endif
	if (*p == '\0')
		return (false);
	while (*p != '\0') {
		if (!isdigit(*p))
			return (false);
		p++;
	}
	return (true);
}

intmax_t
pw_checkuid(char *nptr)
{
#ifdef __APPLE__
	const char *errstr = NULL;
	intmax_t id;

	id = strtonum(nptr, LLONG_MIN, LLONG_MAX, &errstr);
	if (errstr != NULL)
		errx(EX_USAGE, "Bad id '%s': %s", nptr, errstr);
	if ((intmax_t)(id_t)id != id)
		errx(EX_USAGE, "Bad id '%s': too large", nptr);
	return (id);
#elif defined(__FreeBSD__)
	return ((intmax_t)pw_checkid(nptr, UID_MAX));
#else
	return ((intmax_t)pw_checkid(nptr, UID_MAX));
#endif
}

intmax_t
pw_checkgid(char *nptr)
{
#ifdef __APPLE__
	const char *errstr = NULL;
	intmax_t id;

	id = strtonum(nptr, LLONG_MIN, LLONG_MAX, &errstr);
	if (errstr != NULL)
		errx(EX_USAGE, "Bad id '%s': %s", nptr, errstr);
	if ((intmax_t)(id_t)id != id)
		errx(EX_USAGE, "Bad id '%s': too large", nptr);
	return (id);
#elif defined(__FreeBSD__)
	return ((intmax_t)pw_checkid(nptr, GID_MAX));
#else
	return ((intmax_t)pw_checkid(nptr, GID_MAX));
#endif
}

struct userconf *
get_userconfig(const char *config)
{
	char defaultcfg[MAXPATHLEN];

	if (config != NULL)
		return (read_userconfig(config));
	snprintf(defaultcfg, sizeof(defaultcfg), "%s/" _PW_CONF, conf.etcpath);
	return (read_userconfig(defaultcfg));
}

int
nis_update(void) {
	pid_t pid;
	int i;

	if (access(_PATH_YP_MAKEFILE, F_OK) != 0)
		errx(EX_UNAVAILABLE, "NIS update requested but '%s' is missing",
		    _PATH_YP_MAKEFILE);

	fflush(NULL);
	if ((pid = fork()) == -1) {
		warn("fork()");
		return (1);
	}
	if (pid == 0) {
		execlp(_PATH_BMAKE, "make", "-C", _PATH_YPDIR "/", (char *)NULL);
		_exit(1);
	}
	if (waitpid(pid, &i, 0) == -1)
		err(EX_OSERR, "waitpid()");
	if (!WIFEXITED(i))
		errx(EX_SOFTWARE, "make did not exit cleanly");
	if ((i = WEXITSTATUS(i)) != 0)
		errx(i, "make exited with status %d", i);
	return (i);
}

static void
metalog_emit_record(const char *path, const char *target, mode_t mode,
    uid_t uid, gid_t gid, int flags)
{
	const char *flagstr, *type;
	int error;

	if (conf.metalog == NULL)
		return;

	if (target != NULL)
		type = "link";
	else if (S_ISDIR(mode))
		type = "dir";
	else if (S_ISREG(mode))
		type = "file";
	else
		errx(1, "metalog_emit: unhandled file type for %s", path);

	flagstr = fflagstostr(flags &
	    (UF_IMMUTABLE | UF_APPEND | SF_IMMUTABLE | SF_APPEND));
	if (flagstr == NULL)
		errx(1, "metalog_emit: fflagstostr failed");

	error = fprintf(conf.metalog,
	    "./%s type=%s mode=0%03o uid=%" PW_UID_PRI " gid=%" PW_GID_PRI
	    "%s%s%s%s\n",
	    path, type, mode & ACCESSPERMS, PW_UID_ARG(uid), PW_GID_ARG(gid),
	    target != NULL ? " link=" : "", target != NULL ? target : "",
	    *flagstr != '\0' ? " flags=" : "", *flagstr != '\0' ? flagstr : "");
	if (error < 0)
		errx(1, "metalog_emit: write error");
}

void
metalog_emit(const char *path, mode_t mode, uid_t uid, gid_t gid, int flags)
{
	metalog_emit_record(path, NULL, mode, uid, gid, flags);
}

void
metalog_emit_symlink(const char *path, const char *target, mode_t mode,
    uid_t uid, gid_t gid)
{
	metalog_emit_record(path, target, mode, uid, gid, 0);
}
