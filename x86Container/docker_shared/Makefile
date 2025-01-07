src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = gcc
CFLAGS = -Wall -Wextra -Werror -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -Ofast
LDFLAGS =

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit