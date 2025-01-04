src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = gcc
CFLAGS = -g -Wall -Wextra -Werror -ffixed-r8 -ffixed-r9 -ffixed-r10 -ffixed-r11 -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -O2
LDFLAGS = -g

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit