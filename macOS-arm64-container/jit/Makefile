src = $(wildcard *.c) $(wildcard *.S)
obj = $(src:.c=.o)
obj = $(src:.S=.o)

CC = clang 	# MacOS only supports clang, not gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -O2

LDFLAGS = 

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit