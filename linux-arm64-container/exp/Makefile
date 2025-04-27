CC = clang
CFLAGS = -O0

all: main

main: main.o utility.o
	$(CC) $(CFLAGS) -o main main.o utility.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

utility.o: utility.S
	$(CC) $(CFLAGS) -c utility.S

clean:
	rm -f main *.o
