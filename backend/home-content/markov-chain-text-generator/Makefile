CC := gcc
CFLAGS := -Wall -Wextra -Wpedantic -O2 -I./include
LDFLAGS := -L./lib -Wl,-rpath,'$$ORIGIN/../lib'

markov:
	$(CC) $(CFLAGS) $(LDFLAGS) -o bin/generate generate.c -lhashtable -llinkedlist -lutils 

.PHONY: clean
clean:
	rm bin/*
