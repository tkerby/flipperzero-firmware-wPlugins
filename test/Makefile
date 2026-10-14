# Host unit tests for the Tesla FSD protocol core.
# Compiles fsd_logic/fsd_handler.c on the host — no Flipper SDK / SPI needed.
# test_esp32_core does the same for esp32/.firmware/fsd_handler.cpp (no Arduino).
# test_captive_probe covers esp32/.firmware/captive_probe.h (captive-portal logic).
#
#   make -C test check

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -Wextra -O0 -g
CXX     ?= c++
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O0 -g
ROOT    := ..
ESP32   := $(ROOT)/esp32/.firmware
SRC     := test_fsd_core.c $(ROOT)/fsd_logic/fsd_handler.c $(ROOT)/fsd_logic/fsd_profile.c
ESP32_SRC := test_esp32_core.cpp $(ESP32)/fsd_handler.cpp

test_fsd_core: $(SRC) $(ROOT)/fsd_logic/fsd_handler.h $(ROOT)/fsd_logic/fsd_types.h $(ROOT)/fsd_logic/fsd_checksum.h $(ROOT)/fsd_logic/fsd_can_ops.h $(ROOT)/fsd_logic/fsd_perf.h $(ROOT)/fsd_logic/fsd_state.h $(ROOT)/fsd_logic/fsd_capture.h $(ROOT)/fsd_logic/fsd_profile.h $(ROOT)/fsd_logic/fsd_events.h $(ROOT)/fsd_logic/fsd_blackbox_summary.h $(ROOT)/fsd_logic/fsd_blackbox_filter.h $(ROOT)/fsd_logic/fsd_capability.h $(ROOT)/fsd_logic/fsd_profile_db.h $(ROOT)/fsd_logic/fsd_ota.h $(ROOT)/fsd_logic/fsd_autopark.h
	$(CC) $(CFLAGS) -I$(ROOT)/fsd_logic $(SRC) -o $@ -lm

# esp32/.firmware goes first on the include path so "fsd_handler.h" is the
# firmware header, not fsd_logic/fsd_handler.h.
test_esp32_core: $(ESP32_SRC) $(ESP32)/fsd_handler.h $(ESP32)/config.h $(ESP32)/can_signals.h $(ROOT)/fsd_logic/fsd_types.h $(ROOT)/fsd_logic/fsd_state.h $(ROOT)/fsd_logic/fsd_checksum.h $(ROOT)/fsd_logic/fsd_can_ops.h $(ROOT)/fsd_logic/fsd_ota.h $(ROOT)/fsd_logic/fsd_autopark.h  $(ROOT)/fsd_logic/fsd_perf.h $(ROOT)/fsd_logic/fsd_cp_status.h
	$(CXX) $(CXXFLAGS) -I$(ESP32) -I$(ROOT)/fsd_logic $(ESP32_SRC) -o $@

test_can_capture_filter: test_can_capture_filter.cpp $(ESP32)/can_capture_filter.h $(ESP32)/can_driver.h $(ESP32)/fsd_handler.h $(ESP32)/config.h $(ROOT)/fsd_logic/fsd_types.h
	$(CXX) $(CXXFLAGS) -I$(ESP32) -I$(ROOT)/fsd_logic test_can_capture_filter.cpp -o $@

test_captive_probe: test_captive_probe.cpp $(ESP32)/captive_probe.h
	$(CXX) $(CXXFLAGS) -I$(ESP32) test_captive_probe.cpp -o $@

check: test_fsd_core test_esp32_core test_can_capture_filter test_captive_probe
	./test_fsd_core
	./test_esp32_core
	./test_can_capture_filter
	./test_captive_probe

clean:
	rm -rf test_fsd_core test_fsd_core.dSYM test_esp32_core test_esp32_core.dSYM test_can_capture_filter test_can_capture_filter.dSYM test_captive_probe test_captive_probe.dSYM

.PHONY: check clean
