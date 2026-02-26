/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * This is a Darwin userspace shim for FreeBSD's chflagsat(2).
 */

#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(__APPLE__)

#ifdef AT_EMPTY_PATH
#define	PW_AT_EMPTY_PATH	AT_EMPTY_PATH
#else
#define	PW_AT_EMPTY_PATH	0x4000
#endif

static int
pw_chflags_path(const char *path, uint32_t flags, int atflag)
{

	if ((atflag & AT_SYMLINK_NOFOLLOW) != 0)
		return (lchflags(path, flags));
	return (chflags(path, flags));
}

int
chflagsat(int fd, const char *path, unsigned long flags, int atflag)
{
	char *fullpath;
	char dirpath[PATH_MAX];
	uint32_t dflags;
	size_t dlen, plen;
	int ret;

	if (path == NULL) {
		errno = EFAULT;
		return (-1);
	}

	if ((atflag & ~(AT_SYMLINK_NOFOLLOW | PW_AT_EMPTY_PATH)) != 0) {
		errno = EINVAL;
		return (-1);
	}

	if (flags > UINT32_MAX) {
		errno = EINVAL;
		return (-1);
	}
	dflags = (uint32_t)flags;

	if ((atflag & PW_AT_EMPTY_PATH) != 0 && path[0] == '\0') {
		if (fd == AT_FDCWD)
			return (chflags(".", dflags));
		return (fchflags(fd, dflags));
	}
	if (path[0] == '\0') {
		if (fd == AT_FDCWD)
			return (pw_chflags_path(path, dflags, atflag));
		errno = ENOENT;
		return (-1);
	}

	if (path[0] == '/' || fd == AT_FDCWD)
		return (pw_chflags_path(path, dflags, atflag));

	if (fcntl(fd, F_GETPATH, dirpath) == -1)
		return (-1);

	dlen = strlen(dirpath);
	plen = strlen(path);
	fullpath = malloc(dlen + (dlen > 0 && dirpath[dlen - 1] != '/') + plen + 1);
	if (fullpath == NULL)
		return (-1);
	memcpy(fullpath, dirpath, dlen);
	if (dlen > 0 && dirpath[dlen - 1] != '/')
		fullpath[dlen++] = '/';
	memcpy(fullpath + dlen, path, plen + 1);

	ret = pw_chflags_path(fullpath, dflags, atflag);
	free(fullpath);
	return (ret);
}

#endif /* __APPLE__ */
