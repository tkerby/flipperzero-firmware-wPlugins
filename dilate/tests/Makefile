CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I..
LDFLAGS = -lm

TARGET = test_calculations
SOURCES = test_calculations.c ../calculations.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: all test clean
