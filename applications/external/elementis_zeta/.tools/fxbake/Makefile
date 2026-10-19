# fxbake: the battle effects' artwork (fxref.c, the original per-pixel painters) -> nucleo/files/fx.bin (contract 10.4).
#   make            build the baker and write ../../nucleo/files/fx.bin
#   make verbose    the same, with per-kind sizes
#   make check      fails when files/fx.bin differs from a fresh bake
# Needs a host C compiler (cc); nothing else.  tools/build_assets.sh runs it.
ROOT := ../..
NUC  := $(ROOT)/nucleo
OUT  := $(ROOT)/build/fxbake
BIN  := $(OUT)/fxbake
FXBIN := $(NUC)/files/fx.bin
# -ffp-contract=off (contract 72): no fused multiply-add, so the baked bytes are the same on arm64 clang (the dev Mac, which fuses by default) and x86 gcc (CI); fx.bin is committed
CFLAGS ?= -O2 -std=gnu11 -ffp-contract=off -Wall -Wextra -Wno-unused-parameter -DFXBAKE -I$(NUC) -I.

all: $(FXBIN)
$(BIN): fxbake.c fxref.c $(NUC)/fx_tab.h $(NUC)/battle_fx.h $(NUC)/blit.h $(NUC)/lvl.h | $(OUT)
	cc $(CFLAGS) -o $@ fxbake.c fxref.c -lm
$(FXBIN): $(BIN)
	$(BIN) $@
$(OUT):
	mkdir -p $(OUT)
# fails when files/fx.bin is not what the baker writes now (the art, the tables in fx_tab.h or the baker changed without a re-bake)
check: $(BIN)
	$(BIN) $(OUT)/fx_check.bin
	cmp -s $(OUT)/fx_check.bin $(FXBIN) || { echo "fxbake: $(FXBIN) is stale: run make -C tools/fxbake"; exit 1; }
	@echo "fxbake check: fx.bin is up to date"
verbose: $(BIN)
	$(BIN) $(FXBIN) -v
.PHONY: all verbose check
