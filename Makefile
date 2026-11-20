CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -O0

.PHONY: test all bench ubsan clean
.DEFAULT_GOAL := test

test: test_myalloc
	./test_myalloc

all: test_myalloc

test_myalloc: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o $@ myalloc.c test_myalloc.c

bench: bench.c myalloc.c myalloc.h
	$(CC) $(CFLAGS) -O2 -o bench bench.c myalloc.c
	./bench

ubsan: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -fsanitize=undefined -fno-sanitize-recover=all -o test_myalloc_ubsan myalloc.c test_myalloc.c
	./test_myalloc_ubsan

clean:
	rm -f test_myalloc test_myalloc_ubsan bench *.o
