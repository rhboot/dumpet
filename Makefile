
dumpet : dumpet.o
	$(CC) -g3 -O2 -Wall -Werror -o $@ $^

%.o : %.c
	$(CC) -g3 -O2 -Wall -Werror -c -o $@ $^
