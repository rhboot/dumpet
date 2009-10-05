
VERSION=1.0
GITVERSION=$(shell [ -d .git ] && git rev-list  --abbrev-commit  -n 1 HEAD  |cut -b 1-8)

all : dumpet

dumpet : dumpet.o
	$(CC) -g3 -O2 -Wall -Werror -o $@ $^ -lpopt

dumpet.o : dumpet.c dumpet.h iso9660.h eltorito.h endian.h
	$(CC) -g3 -O2 -Wall -Werror -c -o $@ $<

clean : 
	@rm -vf *.o dumpet

install : all
	install -m 0755 dumpet ${DESTDIR}/usr/bin/dumpet

test-archive: clean all dumpet-$(VERSION)-$(GITVERSION).tar.bz2

archive: clean all dumpet-$(VERSION).tar.bz2

dist: tag archive

tag:
	git tag $(VERSION) refs/heads/master

dumpet-$(VERSION).tar.bz2:
	git archive --format=tar $(VERSION) --prefix=dumpet-$(VERSION)/ |bzip2 > dumpet-$(VERSION).tar.bz2

dumpet-$(VERSION)-$(GITVERSION).tar.bz2:
	git archive --format=tar HEAD --prefix=dumpet-$(VERSION)-$(GITVERSION)/ |bzip2 > dumpet-$(VERSION)-$(GITVERSION).tar.bz2

.PHONY : all install clean
