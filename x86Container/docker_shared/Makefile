# Source files excluding um_utils.c
# src = $(filter-out um_utils.c, $(wildcard *.c))
# obj = $(src:.c=.o)

# # Compiler and flags
# CC = gcc
# CFLAGS = -g -Wall -Wextra -Werror -fPIC -ffixed-r8 -ffixed-r9 -ffixed-r10 -ffixed-r11 -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15
# LDFLAGS = -g

# # Add the library path to runtime path
# LDFLAGS += -L. -lum_utils -Wl,-rpath,.

# # Default target
# all: libum_utils.so jit

# # Main program
# jit: $(obj) libum_utils.so
# 	$(CC) $(CFLAGS) -o $@ $(obj) $(LDFLAGS)

# # Shared library target
# libum_utils.so: um_utils.o
# 	$(CC) -shared -o $@ $^

# # Clean target
# clean:
# 	rm -f *.o jit libum_utils.so

# src = $(filter-out um_utils.c, $(wildcard *.c))
src = $(wildcard *.c)
obj = $(src:.c=.o)

# O2 Messes things up too much
CC = gcc
CFLAGS = -g -Wall -Wextra -Werror -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -O2
# CFLAGS = -g -Wall -Wextra -Werror -ffixed-r11 -ffixed-r12 -ffixed-r13 -ffixed-r14 -ffixed-r15 -O2
LDFLAGS = -g

jit: $(obj)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o jit