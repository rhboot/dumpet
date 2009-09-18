
dumpet : dumpet.o
	$(CC) -g3 -O2 -Wall -Werror -o $@ $^ -lpopt

%.o : %.c
	$(CC) -g3 -O2 -Wall -Werror -c -o $@ $^

.PHONY : clean
clean : 
	@rm -vf *.o dumpet
