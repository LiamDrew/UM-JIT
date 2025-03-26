CC = gcc
CFLAGS = -g -Wall -Wextra -Werror -Wpedantic -O2

um: mod_emulator.o virt.o
	$(CC) $(CFLAGS) -o um mod_emulator.o virt.o

mod_emulator.o: mod_emulator.c
	$(CC) $(CFLAGS) -c mod_emulator.c

virt.o: virt.c
	$(CC) $(CFLAGS) -c virt.c


clean:
	rm -f *.o um