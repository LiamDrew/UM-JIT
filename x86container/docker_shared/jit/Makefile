src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = gcc
CFLAGS = -fPIC -Wall -Wextra -Werror -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -O2
LDFLAGS =

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit