.PHONY: pw chkgrp chpass create-out-dir clean install

include pw/sources.mk
include chpass/sources.mk

PREFIX := /usr/local

SYSCONFDIR := /etc

all: pw chkgrp chpass create-out-dir

ifeq ($(shell if [ -d "out" ]; then echo exists; fi),exists)
    has_dir := 1
else
    has_dir :=
endif

create-out-dir: $(if $(has_dir),out)

out:
	mkdir -p out

pw: out/pw

out/pw: $(PW_SRCS:%.c=pw/%.o) $(LIBUTIL_PART:%.c=libutil/%.o)
	$(CC) -o out/pw $^

chkgrp: out/chkgrp

out/chkgrp:
	$(CC) -o out/chkgrp chkgrp/chkgrp.c

chpass: out/chpass

out/chpass: $(CHPASS_SRCS:%.c=chpass/%.o) $(LIBUTIL_PART:%.c=libutil/%.o)
	$(CC) -o out/chpass $^ \
	  -framework CoreFoundation

%.o: %.c
	$(CC) -Ilibutil -Iinclude -D_PATH_ETC="\"$(SYSCONFDIR)\"" -Dst_mtim=st_mtimespec -c $< -o $@

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
	# install chpass
	install -m 0755 out/chpass $(DESTDIR)/$(PREFIX)/bin/chpass
	ln -s chpass $(DESTDIR)/$(PREFIX)/bin/chfn
	ln -s chpass $(DESTDIR)/$(PREFIX)/bin/chsh
	# install doc
	install -d $(DESTDIR)/$(PREFIX)/share/man/man{1,5,8}
	install -m 0644 */*.1 $(DESTDIR)/$(PREFIX)/share/man/man1/
	install -m 0644 */*.5 $(DESTDIR)/$(PREFIX)/share/man/man5/
	install -m 0644 */*.8 $(DESTDIR)/$(PREFIX)/share/man/man8/
