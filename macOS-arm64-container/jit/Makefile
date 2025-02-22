CC = clang
CFLAGS = -Wall -Wextra -Werror -Wpedantic -O2
LDFLAGS =

jit: jit.o utility.o
	$(CC) $(CFLAGS) -o jit jit.o utility.o $(LDFLAGS)

jit.o: jit.c utility.h
	$(CC) $(CFLAGS) -c jit.c

utility.o: utility.S utility.h
	$(CC) -c utility.S

.PHONY: clean
clean:
	rm -f *.o jit