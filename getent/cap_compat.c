/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026
 */

#include <sys/cdefs.h>

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "cap_compat.h"

enum {
	SHIM_RV_OK = 0,
	SHIM_RV_USAGE = 1,
	SHIM_RV_NOTFOUND = 2,
	SHIM_MAX_RECURSION = 32
};

struct cap_iter {
	const char	*db;
	int		expand;
	int		started;
	FILE		*fp;
};

static void	cap_iter_init(struct cap_iter *, const char *, int);
static int	cap_iter_next(struct cap_iter *, char **);
static void	cap_iter_close(struct cap_iter *);

static int	cap_getent(const char *, const char *, int, char **);
static int	cap_getent_expanded(const char *, const char *, char **);
static int	cap_getent_raw(const char *, const char *, char **);

static int	read_raw_record(FILE *, char **);
static int	append_data(char **, size_t *, const char *);
static int	record_matches(const char *, const char *);

static int	handle_one(const char *, char *, int, int, int, int);
static void	prettyprint(char *);
static void	capprint(const char *);

static void
cap_iter_init(struct cap_iter *iter, const char *db, int expand)
{
	assert(iter != NULL);
	iter->db = db;
	iter->expand = expand;
	iter->started = 0;
	iter->fp = NULL;
}

static int
cap_iter_next(struct cap_iter *iter, char **buf)
{
	char *db_array[2] = { NULL, NULL };
	int rc;

	assert(iter != NULL);
	assert(buf != NULL);

	if (iter->expand) {
		db_array[0] = (char *)iter->db;
		if (!iter->started) {
			cgetclose();
			rc = cgetfirst(buf, db_array);
			iter->started = 1;
		} else {
			rc = cgetnext(buf, db_array);
		}
		if (rc < 0)
			return (-1);
		return (rc == 0 ? 0 : 1);
	}

	if (!iter->started) {
		iter->fp = fopen(iter->db, "r");
		if (iter->fp == NULL)
			return (-1);
		iter->started = 1;
	}

	return (read_raw_record(iter->fp, buf));
}

static void
cap_iter_close(struct cap_iter *iter)
{
	assert(iter != NULL);

	if (iter->expand) {
		cgetclose();
		return;
	}

	if (iter->fp != NULL) {
		fclose(iter->fp);
		iter->fp = NULL;
	}
}

static int
cap_getent(const char *db, const char *name, int expand, char **buf)
{
	assert(db != NULL);
	assert(name != NULL);
	assert(buf != NULL);

	if (expand)
		return (cap_getent_expanded(db, name, buf));
	return (cap_getent_raw(db, name, buf));
}

static int
cap_getent_expanded(const char *db, const char *name, char **buf)
{
	char *db_array[2] = { (char *)db, NULL };
	int rc;

	rc = cgetent(buf, db_array, name);
	if (rc >= 0)
		return (0);
	if (rc == -1)
		return (-1);
	return (-2);
}

static int
cap_getent_raw(const char *db, const char *name, char **buf)
{
	FILE *fp;
	char *entry;
	int rc;

	fp = fopen(db, "r");
	if (fp == NULL)
		return (-2);

	for (;;) {
		entry = NULL;
		rc = read_raw_record(fp, &entry);
		if (rc <= 0)
			break;

		if (record_matches(entry, name)) {
			fclose(fp);
			*buf = entry;
			return (0);
		}
		free(entry);
	}

	fclose(fp);
	if (rc < 0)
		return (-2);
	return (-1);
}

static int
append_data(char **dst, size_t *dst_len, const char *src)
{
	size_t src_len;
	size_t new_len;
	char *tmp;

	assert(dst != NULL);
	assert(dst_len != NULL);
	assert(src != NULL);

	src_len = strlen(src);
	if (src_len > SIZE_MAX - *dst_len - 1) {
		errno = EOVERFLOW;
		return (-1);
	}

	new_len = *dst_len + src_len;
	tmp = realloc(*dst, new_len + 1);
	if (tmp == NULL)
		return (-1);

	memcpy(tmp + *dst_len, src, src_len + 1);
	*dst = tmp;
	*dst_len = new_len;
	return (0);
}

static int
read_raw_record(FILE *fp, char **buf)
{
	char *line;
	size_t line_cap;
	ssize_t nread;
	char *record;
	size_t record_len;
	int continue_line;

	assert(fp != NULL);
	assert(buf != NULL);

	line = NULL;
	line_cap = 0;
	record = NULL;
	record_len = 0;
	continue_line = 0;

	for (;;) {
		char *p;

		errno = 0;
		nread = getline(&line, &line_cap, fp);
		if (nread < 0) {
			if (record != NULL) {
				free(line);
				*buf = record;
				return (1);
			}
			free(line);
			if (feof(fp))
				return (0);
			return (-1);
		}

		while (nread > 0 &&
		    (line[nread - 1] == '\n' || line[nread - 1] == '\r')) {
			line[--nread] = '\0';
		}

		if (!continue_line) {
			for (p = line; *p != '\0' &&
			    isspace((unsigned char)*p); p++) {
				/* skip */
			}
			if (*p == '\0' || *p == '#')
				continue;
		}

		continue_line = (nread > 0 && line[nread - 1] == '\\');
		if (continue_line)
			line[nread - 1] = '\0';

		if (append_data(&record, &record_len, line) != 0) {
			free(line);
			free(record);
			return (-1);
		}

		if (!continue_line) {
			free(line);
			*buf = record;
			return (1);
		}
	}
}

static int
record_matches(const char *record, const char *name)
{
	const char *field;
	const char *sep;
	size_t name_len;

	assert(record != NULL);
	assert(name != NULL);

	name_len = strlen(name);
	for (field = record; *field != '\0' && *field != ':'; field = sep + 1) {
		size_t field_len;

		sep = field;
		while (*sep != '\0' && *sep != ':' && *sep != '|')
			sep++;
		field_len = (size_t)(sep - field);

		if (field_len == name_len && strncmp(field, name, field_len) == 0)
			return (1);
		if (*sep == ':')
			break;
	}
	return (0);
}

static void
capprint(const char *cap)
{
	char *c;

	c = strchr(cap, ':');
	if (c != NULL) {
		if (c == cap) {
			printf("true\n");
		} else {
			int l = (int)(c - cap);

			printf("%*.*s\n", l, l, cap);
		}
		return;
	}

	printf("%s\n", cap);
}

static void
prettyprint(char *record)
{
#define TERMWIDTH 65
	int did = 0;
	size_t len;
	char *s;
	char c;

	for (;;) {
		len = strlen(record);
		if (len <= TERMWIDTH) {
done:
			if (did)
				printf("\t:");
			printf("%s\n", record);
			return;
		}

		for (s = record + TERMWIDTH; s > record && *s != ':'; s--)
			continue;
		if (*s++ != ':')
			goto done;

		c = *s;
		*s = '\0';
		if (did)
			printf("\t:");
		did++;
		printf("%s\\\n", record);
		*s = c;
		record = s;
	}
}

static int
handle_one(const char *db, char *record, int expand, int recurse, int pretty,
    int depth)
{
	char *tc;
	char *parent;
	int rc;

	assert(db != NULL);
	assert(record != NULL);

	if (depth > SHIM_MAX_RECURSION)
		return (SHIM_RV_NOTFOUND);

	if (depth > 0 && pretty)
		printf("\n");
	if (pretty)
		prettyprint(record);
	else
		printf("%s\n", record);

	if (!recurse || cgetstr(record, "tc", &tc) <= 0)
		return (SHIM_RV_OK);

	rc = cap_getent(db, tc, expand, &parent);
	free(tc);
	if (rc != 0)
		return (SHIM_RV_OK);

	rc = handle_one(db, parent, expand, recurse, pretty, depth + 1);
	free(parent);
	return (rc);
}

int
cap_compat_handle(const char *db, int argc, char *argv[])
{
	static const char sfx[] = "=#:";
	struct cap_iter iter;
	char *record;
	char *cap;
	int i, c;
	size_t j;
	int expand, recurse, pretty;
	int rc;

	assert(db != NULL);
	assert(argc > 1);
	assert(argv != NULL);

	expand = 1;
	recurse = 0;
	pretty = 0;

	opterr = 0;
	optind = 2;
	while ((c = getopt(argc, argv, "npr")) != -1) {
		switch (c) {
		case 'n':
			expand = 0;
			break;
		case 'r':
			expand = 0;
			recurse = 1;
			break;
		case 'p':
			pretty = 1;
			break;
		default:
			fprintf(stderr,
			    "Usage: %s %s [-npr] [name [capability ...]]\n",
			    argv[0], argv[1]);
			return (SHIM_RV_USAGE);
		}
	}

	if (optind >= argc) {
		cap_iter_init(&iter, db, expand);
		for (;;) {
			record = NULL;
			rc = cap_iter_next(&iter, &record);
			if (rc <= 0)
				break;
			(void)handle_one(db, record, expand, recurse, pretty, 0);
			free(record);
		}
		cap_iter_close(&iter);
		if (rc < 0)
			return (SHIM_RV_NOTFOUND);
		return (SHIM_RV_OK);
	}

	record = NULL;
	if (cap_getent(db, argv[optind], expand, &record) != 0)
		return (SHIM_RV_NOTFOUND);
	optind++;

	if (optind >= argc) {
		rc = handle_one(db, record, expand, recurse, pretty, 0);
		free(record);
		return (rc);
	}

	for (i = optind; i < argc; i++) {
		for (j = 0; j < sizeof(sfx) - 1; j++) {
			cap = cgetcap(record, argv[i], sfx[j]);
			if (cap != NULL) {
				capprint(cap);
				break;
			}
		}
		if (j == sizeof(sfx) - 1)
			printf("false\n");
	}

	free(record);
	return (SHIM_RV_OK);
}
