
all : dumpet

dumpet : dumpet.o
	$(CC) -g3 -O2 -Wall -Werror -o $@ $^ -lpopt

dumpet.o : dumpet.c dumpet.h iso9660.h eltorito.h endian.h
	$(CC) -g3 -O2 -Wall -Werror -c -o $@ $<

clean : 
	@rm -vf *.o dumpet

install : all
	install -m 0755 dumpet ${DESTDIR}/usr/bin/dumpet

.PHONY : all install clean
