# Sanitizer flags for development builds
SANITIZER_FLAGS = -fsanitize=address,undefined \
                  -fno-omit-frame-pointer \
                  -fno-optimize-sibling-calls \
                  -fsanitize-address-use-after-scope \
                  -fsanitize=leak \
                  -fsanitize=integer \
                  -fsanitize=bounds \
                  -fsanitize=null \
                  -fsanitize=return \
                  -fsanitize=signed-integer-overflow \
                  -fsanitize=unreachable \
                  -fsanitize=vla-bound

# Use for debug builds only (not for Flipper device)
debug-sanitized:
	gcc $(SANITIZER_FLAGS) -g -O0 govee_control/*.c -o govee_test