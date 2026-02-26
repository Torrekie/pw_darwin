/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 1996
 *	David L. Nugent.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY DAVID L. NUGENT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL DAVID L. NUGENT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <pwd.h>
#include <libutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pwupd.h"

char *
getpwpath(char const * file)
{
	static char pathbuf[MAXPATHLEN];

	snprintf(pathbuf, sizeof pathbuf, "%s/%s", conf.etcpath, file);

	return (pathbuf);
}

static struct passwd *
pw_lookup_master(const char *name)
{
	FILE *fp;
	char *line = NULL;
	size_t linecap = 0;
	ssize_t linelen;
	struct passwd *pw = NULL;

	if (name == NULL)
		return (NULL);
	fp = fopen(getpwpath(_MASTERPASSWD), "r");
	if (fp == NULL)
		return (NULL);

	while ((linelen = getline(&line, &linecap, fp)) > 0) {
		if (line[0] == '\n' || line[0] == '#')
			continue;
		if (line[linelen - 1] == '\n')
			line[linelen - 1] = '\0';
		pw = pw_scan(line, PWSCAN_MASTER);
		if (pw == NULL)
			continue;
		if (strcmp(pw->pw_name, name) == 0)
			break;
		free(pw);
		pw = NULL;
	}

	free(line);
	fclose(fp);
	return (pw);
}

static int
pwdb_check(void)
{
	int             i = 0;
	pid_t           pid;
	char           *args[10];

	args[i++] = _PATH_PWD_MKDB;
#ifdef __APPLE__
	args[i++] = "-c";
#else
	args[i++] = "-C";
#endif

	if (strcmp(conf.etcpath, _PATH_PWD) != 0) {
		args[i++] = "-d";
		args[i++] = conf.etcpath;
	}
	args[i++] = getpwpath(_MASTERPASSWD);
	args[i] = NULL;

	if ((pid = fork()) == -1)	/* Error (errno set) */
		i = errno;
	else if (pid == 0) {	/* Child */
		execv(args[0], args);
		_exit(1);
	} else {		/* Parent */
		waitpid(pid, &i, 0);
		if (WEXITSTATUS(i))
			i = EIO;
	}

	return (i);
}

static int
pw_update(struct passwd * pwd, char const * user)
{
	struct passwd	*pw = NULL;
	struct passwd	*old_pw = NULL;
	int		 rc, pfd, tfd;

	if ((rc = pwdb_check()) != 0)
		return (rc);

	if (pwd != NULL)
		pw = pw_dup(pwd);

	if (user != NULL)
		old_pw = pw_lookup_master(user);

	if (pw_init(conf.etcpath, NULL))
		err(1, "pw_init()");
	if ((pfd = pw_lock()) == -1) {
		pw_fini();
		err(1, "pw_lock()");
	}
	if ((tfd = pw_tmp(-1)) == -1) {
		pw_fini();
		err(1, "pw_tmp()");
	}
	if (pw_copy(pfd, tfd, pw, old_pw) == -1) {
		pw_fini();
		close(tfd);
		err(1, "pw_copy()");
	}
	fsync(tfd);
	close(tfd);
	/*
	 * in case of deletion of a user, the whole database
	 * needs to be regenerated
	 */
	if (pw_mkdb(pw != NULL ? pw->pw_name : NULL) == -1) {
		pw_fini();
		err(1, "pw_mkdb()");
	}
	free(old_pw);
	free(pw);
	pw_fini();

	return (0);
}

int
addpwent(struct passwd * pwd)
{

	return (pw_update(pwd, NULL));
}

int
chgpwent(char const * login, struct passwd * pwd)
{

	return (pw_update(pwd, login));
}

int
delpwent(struct passwd * pwd)
{

	return (pw_update(NULL, pwd->pw_name));
}
