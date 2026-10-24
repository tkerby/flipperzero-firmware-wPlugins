CC ?= cc
CFLAGS ?= -std=c11 -Wall -Wextra -Werror

SOURCES = test_devboard_protocol.c ../devboard_protocol.c
HEADERS = ../devboard_protocol.h

.PHONY: test clean

test: test_devboard_protocol
	./test_devboard_protocol

test_devboard_protocol: $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f test_devboard_protocol
