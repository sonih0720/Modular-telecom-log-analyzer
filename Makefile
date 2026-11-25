CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -Wpedantic -O2
CPPFLAGS := -Iinclude
TARGET  := analyzer

SRC := $(wildcard src/*.c)
OBJ := $(SRC:src/%.c=build/%.o)

.PHONY: all debug test clean

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^

build/%.o: src/%.c $(wildcard include/*.h) | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $< -o $@

build:
	mkdir -p build

# Debug build: symbols for GDB plus AddressSanitizer to catch memory bugs
debug: clean
	$(MAKE) CFLAGS="-std=c11 -Wall -Wextra -Wpedantic -g -O0 -fsanitize=address,undefined"

test: $(TARGET)
	bash tests/run_tests.sh

clean:
	rm -rf build $(TARGET)
