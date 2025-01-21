CC = gcc
OBJECT = return.o
EXECUTABLE = return

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJECT)
	$(CC) -o $@ $^

return.o: return.s
	$(CC) -c return.s -o return.o

clean:
	rm -f $(OBJECT) $(EXECUTABLE)

.PHONY: all clean