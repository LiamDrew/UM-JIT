CC = clang
CFLAGS = -Wall -Wextra -Werror

main: main.o utility.o
	$(CC) $(CLAGS) -o main main.o utility.o

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

utility.o: utility.S utility.h
	$(CC) $(CFLAGS) -c utility.S

clean:
	rm -f main *.o