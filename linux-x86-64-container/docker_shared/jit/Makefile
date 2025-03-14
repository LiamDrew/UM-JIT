src = $(wildcard *.c) $(wildcard *.S)
obj = $(src:.c=.o)
obj = $(src:.S=.o)

CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -O2

LDFLAGS =

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit