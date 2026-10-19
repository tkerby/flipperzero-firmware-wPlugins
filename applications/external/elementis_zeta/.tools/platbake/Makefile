# platbake: the battle platforms (plat_ref.h, the original builder) -> nucleo/files/plat.bin (contract 73).
#   make            build the baker and write ../../nucleo/files/plat.bin
#   make check      fails when files/plat.bin differs from a fresh bake
# Needs a host C compiler (cc); nothing else.  tools/build_assets.sh runs it.
ROOT := ../..
NUC  := $(ROOT)/nucleo
OUT  := $(ROOT)/build/platbake
BIN  := $(OUT)/platbake
PLAT := $(NUC)/files/plat.bin
# -ffp-contract=off: the shadow's radii are float products (as in fxbake): no fused multiply-add, so the bytes are the same on arm64 clang and x86 gcc
CFLAGS ?= -O2 -std=gnu11 -ffp-contract=off -Wall -Wextra -I.

all: $(PLAT)
$(BIN): platbake.c plat_ref.h | $(OUT)
	cc $(CFLAGS) -o $@ platbake.c
$(PLAT): $(BIN)
	$(BIN) $@
$(OUT):
	mkdir -p $(OUT)
check: $(BIN)
	$(BIN) $(OUT)/plat_check.bin > /dev/null
	cmp -s $(OUT)/plat_check.bin $(PLAT) || { echo "platbake: $(PLAT) is stale: run make -C tools/platbake"; exit 1; }
	@echo "platbake check: plat.bin is up to date"
.PHONY: all check
