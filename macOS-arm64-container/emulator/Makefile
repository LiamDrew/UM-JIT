src = $(wildcard *.c)
obj = $(src:.c=.o)

# How is clang so much slower than gcc? What?
# If Terry Davis is correct, this means that I need to learn how to write
# better C code
CC = clang
CFLAGS = -g -Wall -Wextra -Werror -O2
LDFLAGS = -g

um: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o um
	