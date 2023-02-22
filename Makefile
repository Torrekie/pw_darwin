.PHONY: pw create-out-dir clean install

include pw/sources.mk

PREFIX := /usr/local

SYSCONFDIR := /etc

all: pw create-out-dir

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

%.o: %.c
	$(CC) -Ilibutil -Iinclude -D_PATH_ETC="\"$(SYSCONFDIR)\"" -Dst_mtim=st_mtimespec -c $< -o $@

clean:
	rm -f pw/*.o libutil/*.o out/*

install: pw
	# install pw
	install -d $(DESTDIR)/$(PREFIX)/bin
	install -m 0755 out/pw $(DESTDIR)/$(PREFIX)/bin/pw
	# install doc
	install -d $(DESTDIR)/$(PREFIX)/share/man/man{5,8}
	install -m 0644 pw/$(filter %.5,$(MAN)) $(DESTDIR)/$(PREFIX)/share/man/man5/
	install -m 0644 pw/$(filter %.8,$(MAN)) $(DESTDIR)/$(PREFIX)/share/man/man8/
