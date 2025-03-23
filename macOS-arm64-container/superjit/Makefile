CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -O2
LDFLAGS =

jit: jit.o utility.o virt.o
	$(CC) $(CFLAGS) -o jit jit.o utility.o virt.o $(LDFLAGS)

jit.o: jit.c utility.h
	$(CC) $(CFLAGS) -c jit.c

utility.o: utility.S utility.h
	$(CC) -c utility.S

virt.o: virt.c virt.h
	$(CC) -c virt.c

.PHONY: clean
clean:
	rm -f *.o jit