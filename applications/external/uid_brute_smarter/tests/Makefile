# Makefile for UID Brute Smarter Testing Suite
# Cross-platform unit testing without Flipper Zero hardware

# Compiler settings
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g -O0 -I../ -I../unity -I./mocks -DUNIT_TEST
LDFLAGS = 

# Add coverage flags if not disabled
ifdef NO_COVERAGE
    CFLAGS += 
    LDFLAGS += 
else
    CFLAGS += --coverage -fprofile-arcs -ftest-coverage
    LDFLAGS += -lgcov --coverage
endif

# Build directories
BUILD_DIR = build
COVERAGE_DIR = coverage

# Default target
.PHONY: all clean test test-pattern test-nfc test-key coverage help dev-setup

all: test

# Development setup
dev-setup:
	@echo "🔧 Setting up development environment..."
	@command -v gcc >/dev/null 2>&1 || (echo "❌ gcc not found"; exit 1)
	@command -v make >/dev/null 2>&1 || (echo "❌ make not found"; exit 1)
	@command -v lcov >/dev/null 2>&1 || (echo "❌ lcov not found - install with: sudo apt-get install lcov"; exit 1)
	@command -v bc >/dev/null 2>&1 || (echo "❌ bc not found - install with: sudo apt-get install bc"; exit 1)
	@echo "✅ Development environment ready!"

# Build dependencies
unity/unity.o: unity/unity.c unity/unity.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c unity/unity.c -o unity/unity.o

mocks/furi_mock.o: mocks/furi_mock.c mocks/furi_mock.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c mocks/furi_mock.c -o mocks/furi_mock.o

../pattern_engine.o: ../pattern_engine.c ../pattern_engine.h
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -c ../pattern_engine.c -o ../pattern_engine.o

# Pattern engine tests
test-pattern: unity/unity.o mocks/furi_mock.o ../pattern_engine.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_pattern_engine.c unity/unity.o mocks/furi_mock.o ../pattern_engine.o -o $(BUILD_DIR)/test_pattern_engine
	./$(BUILD_DIR)/test_pattern_engine

# NFC parser tests
test-nfc: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_nfc_parser.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_nfc_parser
	./$(BUILD_DIR)/test_nfc_parser

# Key management tests
test-key: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_key_management.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_key_management
	./$(BUILD_DIR)/test_key_management

# Menu navigation tests
test-menu: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_menu_navigation.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_menu_navigation
	./$(BUILD_DIR)/test_menu_navigation

# Brute force completion tests
test-brute: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_brute_force_completion.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_brute_force_completion
	./$(BUILD_DIR)/test_brute_force_completion

# UI state management tests
test-ui: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_ui_state_management.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_ui_state_management
	./$(BUILD_DIR)/test_ui_state_management

# Config callback tests
test-config: unity/unity.o mocks/furi_mock.o
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) unit/test_config_callback.c unity/unity.o mocks/furi_mock.o -o $(BUILD_DIR)/test_config_callback
	./$(BUILD_DIR)/test_config_callback

# Run all tests
test: test-pattern test-nfc test-key test-menu test-brute test-ui test-config
	@echo "✅ All tests completed successfully!"

# Generate coverage report
coverage: test
	@mkdir -p $(COVERAGE_DIR)
	@echo "📊 Generating coverage report..."
	
	# Generate coverage data
	gcov ../pattern_engine.c -o ../
	
	# Generate lcov report if available
	@if command -v lcov >/dev/null 2>&1; then \
		lcov --capture --directory .. --output-file $(COVERAGE_DIR)/coverage.info --include "*/pattern_engine.c" --include "*/unit/*"; \
		lcov --remove $(COVERAGE_DIR)/coverage.info '/usr/*' --output-file $(COVERAGE_DIR)/coverage.info; \
		echo "Coverage info generated: $(COVERAGE_DIR)/coverage.info"; \
		if command -v genhtml >/dev/null 2>&1; then \
			genhtml $(COVERAGE_DIR)/coverage.info --output-directory $(COVERAGE_DIR)/html; \
			echo "📈 HTML coverage report generated at: $(COVERAGE_DIR)/html/index.html"; \
		fi \
	else \
		echo "⚠️  lcov not found, coverage summary only"; \
	fi

# Check coverage summary
coverage-check:
	@if [ -f $(COVERAGE_DIR)/coverage.info ]; then \
		echo "Coverage Summary:"; \
		lcov --summary $(COVERAGE_DIR)/coverage.info 2>/dev/null; \
	else \
		echo "Run 'make coverage' first to generate coverage data"; \
	fi

# Clean build artifacts
clean:
	@echo "🧹 Cleaning build artifacts..."
	@rm -rf $(BUILD_DIR) $(COVERAGE_DIR)
	@rm -f ../pattern_engine.gcda ../pattern_engine.gcno ../pattern_engine.gcov
	@rm -f unity/unity.gcda unity/unity.gcno unity/unity.gcov
	@rm -f mocks/furi_mock.gcda mocks/furi_mock.gcno mocks/furi_mock.gcov
	@rm -f unit/*.gcda unit/*.gcno unit/*.gcov
	@echo "✅ Cleanup completed"

# Help
help:
	@echo "UID Brute Smarter Testing Suite"
	@echo "==============================="
	@echo ""
	@echo "Targets:"
	@echo "  make dev-setup     - Check development dependencies"
	@echo "  make test          - Run all unit tests"
	@echo "  make test-pattern  - Run pattern engine tests only"
	@echo "  make test-nfc      - Run NFC parser tests only"
	@echo "  make test-key      - Run key management tests only"
	@echo "  make test-menu     - Run menu navigation tests only"
	@echo "  make test-config   - Run config callback tests only"
	@echo "  make test-brute    - Run brute force completion tests only"
	@echo "  make test-ui       - Run UI state management tests only"
	@echo "  make coverage      - Run tests and generate coverage report"
	@echo "  make coverage-check - Show coverage summary"
	@echo "  make clean         - Clean build artifacts"
	@echo ""
	@echo "Quick start:"
	@echo "  make dev-setup && make test && make coverage"