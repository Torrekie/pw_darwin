.PHONY: pw chkgrp create-out-dir clean install

include pw/sources.mk

PREFIX := /usr/local

SYSCONFDIR := /etc
LIBUTIL_PREFIX ?= /usr/local
LIBUTIL_INCLUDE ?= $(LIBUTIL_PREFIX)/include
LIBUTIL_LIBDIR ?= $(LIBUTIL_PREFIX)/lib

CPPFLAGS += -Iinclude -I$(LIBUTIL_INCLUDE) -D_PATH_ETC="\"$(SYSCONFDIR)\"" -Dst_mtim=st_mtimespec
LDFLAGS += -L$(LIBUTIL_LIBDIR) -Wl,-rpath,$(LIBUTIL_LIBDIR)
PW_LIBS ?= -lutil-fbsd -lcrypt-fbsd

all: pw chkgrp create-out-dir

ifeq ($(shell if [ -d "out" ]; then echo exists; fi),exists)
    has_dir := 1
else
    has_dir :=
endif

create-out-dir: $(if $(has_dir),out)

out:
	mkdir -p out

pw: out/pw

out/pw: $(PW_SRCS:%.c=pw/%.o)
	$(CC) $(LDFLAGS) -o out/pw $^ $(PW_LIBS)

chkgrp: out/chkgrp

out/chkgrp:
	$(CC) -o out/chkgrp chkgrp/chkgrp.c

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -f */*.o out/*

install: all
	# install pw
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -d $(DESTDIR)/$(PREFIX)/sbin
	install -m 0755 out/pw $(DESTDIR)/$(PREFIX)/sbin/pw
	# install adduser
	install -m 0755 adduser/adduser.sh $(DESTDIR)/$(PREFIX)/sbin/adduser
	install -m 0755 adduser/rmuser.sh $(DESTDIR)/$(PREFIX)/sbin/rmuser
	# install chkgrp
	install -m 0755 out/chkgrp $(DESTDIR)/$(PREFIX)/sbin/chkgrp
	# install vigr
	install -m 0755 vigr/vigr.sh $(DESTDIR)/$(PREFIX)/sbin/vigr
	# install doc
	install -d $(DESTDIR)/$(PREFIX)/share/man/man{1,5,8}
	install -m 0644 */*.1 $(DESTDIR)/$(PREFIX)/share/man/man1/
	install -m 0644 */*.5 $(DESTDIR)/$(PREFIX)/share/man/man5/
	install -m 0644 */*.8 $(DESTDIR)/$(PREFIX)/share/man/man8/
