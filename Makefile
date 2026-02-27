.PHONY: all clean install create-out-dir pw chkgrp getent logins

include pw/sources.mk

# Build configuration.
PREFIX ?= /usr/local
SYSCONFDIR ?= /etc
OUTDIR ?= out

LIBUTIL_PREFIX ?= /usr/local
LIBUTIL_INCLUDE ?= $(LIBUTIL_PREFIX)/include
LIBUTIL_LIBDIR ?= $(LIBUTIL_PREFIX)/lib

CPPFLAGS += \
	-Iinclude \
	-I$(LIBUTIL_INCLUDE) \
	-D_PATH_ETC="\"$(SYSCONFDIR)\"" \
	-Dst_mtim=st_mtimespec
LDFLAGS += \
	-L$(LIBUTIL_LIBDIR) \
	-Wl,-rpath,$(LIBUTIL_LIBDIR)
PW_LIBS ?= -lutil-fbsd -lcrypt-fbsd

# Install paths.
BIN_DIR := $(DESTDIR)$(PREFIX)/bin
SBIN_DIR := $(DESTDIR)$(PREFIX)/sbin
MAN1_DIR := $(DESTDIR)$(PREFIX)/share/man/man1
MAN5_DIR := $(DESTDIR)$(PREFIX)/share/man/man5
MAN8_DIR := $(DESTDIR)$(PREFIX)/share/man/man8

PW_OBJS := $(PW_SRCS:%.c=pw/%.o)

all: pw chkgrp logins

create-out-dir: | $(OUTDIR)

pw: $(OUTDIR)/pw

chkgrp: $(OUTDIR)/chkgrp

getent: $(OUTDIR)/getent

logins: $(OUTDIR)/logins

$(OUTDIR):
	mkdir -p $@

$(OUTDIR)/pw: $(PW_OBJS) | $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $^ $(PW_LIBS)

$(OUTDIR)/chkgrp: chkgrp/chkgrp.c | $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $<

$(OUTDIR)/getent: getent/getent.c getent/cap_compat.c | $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $^

$(OUTDIR)/logins: logins/logins.c | $(OUTDIR)
	$(CC) -o $@ $<

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f */*.o $(OUTDIR)/*

install: all getent
	install -d $(BIN_DIR) $(SBIN_DIR)
	install -m 0755 $(OUTDIR)/pw $(SBIN_DIR)/pw
	install -m 0755 adduser/adduser.sh $(SBIN_DIR)/adduser
	install -m 0755 adduser/rmuser.sh $(SBIN_DIR)/rmuser
	install -m 0755 $(OUTDIR)/chkgrp $(SBIN_DIR)/chkgrp
	install -m 0755 $(OUTDIR)/getent $(BIN_DIR)/getent
	install -m 0755 $(OUTDIR)/logins $(BIN_DIR)/logins
	install -m 0755 vigr/vigr.sh $(SBIN_DIR)/vigr
	install -d $(MAN1_DIR) $(MAN5_DIR) $(MAN8_DIR)
	install -m 0644 */*.1 $(MAN1_DIR)/
	install -m 0644 */*.5 $(MAN5_DIR)/
	install -m 0644 */*.8 $(MAN8_DIR)/
