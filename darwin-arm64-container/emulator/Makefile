src = $(wildcard *.c)
obj = $(src:.c=.o)

# How is clang so much slower than gcc? What?
# If Terry Davis is correct, this means that I need to learn how to write
# better C code

# My working assumption: gcc making other optimizations buried in the assembly
# that clang is not doing. Hopefully my JIT compiler will still get the benefit
# of this virtual memory system, and that gcc's optimizations aren't related
# to the memory system we've spend 60 hours building

# TODO: disassemble this program and figure out why gcc is doing so much better
# here. Hopefully I can read the assembly well enough and find some more
# improvements

# CC = clang
CC = gcc-14
CFLAGS = -g -Wall -Wextra -Werror -O2
LDFLAGS = -g

um: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) 

clean:
	rm -f *.o um
	