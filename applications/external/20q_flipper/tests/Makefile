# Desktop harness (SPEC 6.7 / 11.1). The engine has no Furi dependency, so the
# code under test here is byte-for-byte the code that ships in the .fap.
CC      ?= cc
CFLAGS  ?= -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wdouble-promotion \
           -Wundef -Wredundant-decls -fno-common
LDFLAGS ?= -lm
SRC = ../helpers/twentyq_dataset.c ../helpers/twentyq_learned.c \
      ../helpers/twentyq_engine.c tq_file_reader.c

all: test

../build/tq_test: $(SRC) tq_test.c tq_file_reader.h
	@mkdir -p ../build
	$(CC) $(CFLAGS) -o $@ $(SRC) tq_test.c $(LDFLAGS)

golden:
	cd .. && python3 tools/pack.py tests/golden_8x6.dat

test: golden ../build/tq_test
	cd .. && ./build/tq_test

clean:
	rm -f ../build/tq_test ../build/corrupt.dat

.PHONY: all test golden clean

# The simulator links the same engine sources; TQ_EPS and TQ_CANDIDATES can be
# overridden from the command line for the SPEC 11.2 parameter sweep.
SIMFLAGS ?=
../build/tq_sim: $(SRC) tq_sim.c
	@mkdir -p ../build
	$(CC) $(CFLAGS) $(SIMFLAGS) -o $@ $(SRC) tq_sim.c $(LDFLAGS)

sim: ../build/tq_sim
	cd .. && ./build/tq_sim files/20q.dat --games 400

.PHONY: sim
