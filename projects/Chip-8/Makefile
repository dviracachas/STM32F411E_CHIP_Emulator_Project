CC      = gcc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -Icore \
          -fsanitize=address,undefined
SDL     = $(shell pkg-config --cflags --libs sdl2)

build/chip8: platform_sdl/main.c core/chip8.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@ $(SDL)

build/tests: tests/test_chip8.c core/chip8.c
	@mkdir -p build
	$(CC) $(CFLAGS) $^ -o $@

test: build/tests
	./build/tests

clean:
	rm -rf build

.PHONY: test clean