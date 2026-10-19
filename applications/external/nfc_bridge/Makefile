# Host-side build and tests for the portable core (no Flipper needed).
CORE = core/nb_tag.c core/nb_pn532.c core/nb_ccid.c core/nb_virtual.c
TEST = tests/mock_backend.c tests/nbtest.c
CFLAGS = -std=c99 -Wall -Wextra -Wshadow -g -fsanitize=address,undefined -shared -fPIC

test: tests/libnbtest.so
	LD_PRELOAD=$$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=0 python3 tests/test_core.py
	LD_PRELOAD=$$(gcc -print-file-name=libasan.so) ASAN_OPTIONS=detect_leaks=0 python3 tests/test_virtual.py

tests/libnbtest.so: $(CORE) $(TEST) core/*.h tests/*.h
	gcc $(CFLAGS) -o $@ $(CORE) $(TEST)

clean:
	rm -f tests/libnbtest.so

.PHONY: test clean
