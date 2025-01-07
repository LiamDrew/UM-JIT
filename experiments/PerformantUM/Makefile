src = $(wildcard *.c)
obj = $(src:.c=.o)

# NOTE: Clang is the only compiler available on the Mac
CC = clang
CFLAGS = -g -Wall -Wextra -Werror -O2
LDFLAGS = -g

um: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o um