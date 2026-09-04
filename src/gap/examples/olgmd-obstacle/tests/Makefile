CC ?= cc
CFLAGS ?= -O2 -std=c11 -Wall -Wextra -Werror

.PHONY: all test clean

all: test_olgmd1 test_wire

test_olgmd1: test_olgmd1.c ../olgmd1.c ../olgmd1.h
	$(CC) $(CFLAGS) -I.. test_olgmd1.c ../olgmd1.c -o $@

test_wire: test_wire.c ../olgmd_wire.h ../../../lib/crc32.c
	$(CC) $(CFLAGS) -I.. -I../../../lib test_wire.c ../../../lib/crc32.c -o $@

test: test_olgmd1 test_wire
	./test_olgmd1
	./test_wire

clean:
	$(RM) test_olgmd1 test_wire
