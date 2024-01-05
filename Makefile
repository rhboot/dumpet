
VERSION=2.1
GITVERSION=$(shell [ -d .git ] && git rev-list  --abbrev-commit  -n 1 HEAD  |cut -b 1-8)

CFLAGS := -g3 -O2 -Wall -Werror -Wno-unused-but-set-variable --std=gnu99
LFLAGS :=

TOOL := memcheck
PKG_CONFIG := pkg-config

LIBXML_CFLAGS := $(shell $(PKG_CONFIG) --cflags libxml-2.0)
LIBXML_LFLAGS := -lpopt $(shell $(PKG_CONFIG) --libs libxml-2.0)

ifeq ($(shell uname -s),Darwin)
	IS_DARWIN=1
	HAS_BREW=$(shell which brew)
	ifneq ($(HAS_BREW),)
		PREFIX=$(shell brew --prefix)
	endif

	CC=clang

	CFLAGS += -I$(PREFIX)/include -I$(shell pwd)
	LFLAGS += -L$(PREFIX)/lib
endif

all : dumpet test

test : apmtest
	valgrind --tool=$(TOOL) ./apmtest -r apple.mba31.restore.firstmeg.iso 

dumpet : dumpet.o applepart.o
	$(CC) $(CFLAGS) -o $@ $^ $(LFLAGS) -lpopt $(LIBXML_LFLAGS)

apmtest : applepart.c
	$(CC) $(CFLAGS) $(LIBXML_CFLAGS) -DTEST_DUMPER -o $@ $^ $(LFLAGS) -lpopt $(LIBXML_LFLAGS)

dumpet.o : dumpet.c dumpet.h iso9660.h eltorito.h endian.h
	$(CC) $(CFLAGS) $(LIBXML_CFLAGS) -c -o $@ $<

clean : 
	@rm -vf *.o dumpet apmtest

install : all
	install -D -m 0755 dumpet ${DESTDIR}/usr/bin/dumpet
	install -D -m 0644 dumpet.1 ${DESTDIR}/usr/share/man/man1/dumpet.1

test-archive: clean all dumpet-$(VERSION)-$(GITVERSION).tar.bz2

archive: clean all dumpet-$(VERSION).tar.bz2

dist: tag archive

tag:
	git tag $(VERSION) refs/heads/master

dumpet-$(VERSION).tar.bz2:
	git archive --format=tar $(VERSION) --prefix=dumpet-$(VERSION)/ |bzip2 > dumpet-$(VERSION).tar.bz2

dumpet-$(VERSION)-$(GITVERSION).tar.bz2:
	git archive --format=tar HEAD --prefix=dumpet-$(VERSION)-$(GITVERSION)/ |bzip2 > dumpet-$(VERSION)-$(GITVERSION).tar.bz2

upload: dist
	@scp dumpet-$(VERSION).tar.bz2 fedorahosted.org:dumpet

.PHONY : all install clean
