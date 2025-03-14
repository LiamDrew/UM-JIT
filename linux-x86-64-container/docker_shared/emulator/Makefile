src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = gcc-14
CFLAGS = -g -Wall -Wextra -Werror -O2
LDFLAGS = -g

um: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o um
	