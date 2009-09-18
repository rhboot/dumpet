
all : dumpet

dumpet : dumpet.o
	$(CC) -g3 -O2 -Wall -Werror -o $@ $^ -lpopt

dumpet.o : dumpet.c dumpet.h iso9660.h eltorito.h endian.h
	$(CC) -g3 -O2 -Wall -Werror -c -o $@ $<

.PHONY : clean
clean : 
	@rm -vf *.o dumpet
