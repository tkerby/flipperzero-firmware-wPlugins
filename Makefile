PYTHON := .venv/bin/python
UFBT := .venv/bin/ufbt
MOMENTUM_HOME := $(CURDIR)/.ufbt/momentum
MOMENTUM_SDK := https://github.com/Next-Flip/Momentum-Firmware/releases/download/mntm-009/flipper-z-f7-sdk-mntm-009.zip
FIRMWARE ?= official
TOOLCHAIN_HOME := $(HOME)/.ufbt
SDK_HOME := $(if $(filter momentum,$(FIRMWARE)),$(MOMENTUM_HOME),$(TOOLCHAIN_HOME))
SDK_ENV := UFBT_HOME="$(SDK_HOME)" FBT_TOOLCHAIN_PATH="$(TOOLCHAIN_HOME)"
SDK_PYTHON := $(TOOLCHAIN_HOME)/toolchain/current/bin/python3

.PHONY: all setup build songs sfx package test momentum-sdk momentum launch upload-songs upload-sfx
all: package

setup:
	python3 -m venv .venv
	$(PYTHON) -m pip install -r requirements-dev.txt
	$(UFBT) update --channel=release

build:
	$(SDK_ENV) $(UFBT)
	mkdir -p dist/$(FIRMWARE)
	cp dist/taiko_player.fap dist/$(FIRMWARE)/taiko_player.fap

songs:
	$(PYTHON) scripts/prepare_songs.py

sfx:
	$(PYTHON) scripts/prepare_sfx.py

package: build songs sfx
	mkdir -p sdcard/apps/Media
	cp dist/taiko_player.fap sdcard/apps/Media/taiko_player.fap

test:
	$(PYTHON) scripts/test_core.py

# Separate SDK state keeps the official SDK available. Both use uFBT's shared toolchain.
momentum-sdk:
	UFBT_HOME="$(MOMENTUM_HOME)" FBT_TOOLCHAIN_PATH="$(HOME)/.ufbt" $(UFBT) update --hw-target=f7 --url=$(MOMENTUM_SDK)

momentum:
	$(MAKE) build FIRMWARE=momentum

upload-songs: songs
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/storage.py send sdcard/apps_data/taiko_player /ext/apps_data/taiko_player

upload-sfx: sfx
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/storage.py send sdcard/apps_data/taiko_player/sfx /ext/apps_data/taiko_player/sfx

launch: build
	$(SDK_PYTHON) $(TOOLCHAIN_HOME)/current/scripts/runfap.py --sources dist/taiko_player.fap --targets /ext/apps/Media/taiko_player.fap
