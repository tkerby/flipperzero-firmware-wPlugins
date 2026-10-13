# Convenience wrapper. The device build is ufbt; everything else is stdlib
# Python and a plain C compiler.
PY   ?= ./venv/bin/python
UFBT ?= ./venv/bin/ufbt
# Keep the SDK out of the app tree: ufbt globs its own project template
# into the build if UFBT_HOME lives inside the app directory.
export UFBT_HOME ?= $(CURDIR)/.ufbt

.PHONY: all data test sim eval fap launch icon clean distclean

all: data test fap

data:            ## rebuild files/20q.dat from tools/*.yaml
	$(PY) tools/build_dataset.py

test:            ## desktop unit tests against the shipping engine
	$(MAKE) -C tests test

sim:             ## play 400 simulated games
	$(MAKE) -C tests sim

eval:            ## SPEC 11.2 parameter sweep
	$(PY) tools/eval.py

fap:             ## cross-compile dist/twentyq.fap
	$(UFBT)

launch:          ## build, upload and run on a connected Flipper
	$(UFBT) launch

icon:
	$(PY) tools/make_icon.py twentyq.png

clean:
	$(MAKE) -C tests clean
	rm -rf build/tq_test build/tq_sim build/corrupt.dat

distclean: clean
	rm -rf dist .ufbt build
