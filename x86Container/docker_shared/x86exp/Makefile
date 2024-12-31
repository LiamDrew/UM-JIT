src = $(wildcard *.c)
obj = $(src:.c=.o)

CC = gcc
CFLAGS = -g -Wall -Wextra -Werror
LDFLAGS = -g

exp: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o exp