# Enhanced Pokemon Game Makefile for Flipper Zero
# Uses fbt (Flipper Build Tool) or ufbt (micro Flipper Build Tool)

# Project settings
APP_ID = pokemon_yellow_plus
PROJECT_DIR = $(shell pwd)

# Detect build tool (prefer ufbt if available, fallback to fbt)
BUILD_TOOL := $(shell command -v ufbt 2> /dev/null)
ifndef BUILD_TOOL
    BUILD_TOOL := $(shell command -v fbt 2> /dev/null)
    ifndef BUILD_TOOL
        $(error Neither ufbt nor fbt found. Please install Flipper Zero SDK)
    endif
endif

# Build targets
.PHONY: all clean extract_data build install launch debug format lint info help

all: extract_data build

# Extract data from pokeyellow
extract_data:
	@echo "Extracting Pokemon data from pokeyellow..."
	@if [ -f "../pokemon_data_extractor.py" ]; then \
		python3 ../pokemon_data_extractor.py; \
	else \
		echo "Warning: pokemon_data_extractor.py not found, using existing data"; \
	fi
	@if [ -f "../sprite_converter.py" ]; then \
		python3 ../sprite_converter.py; \
	else \
		echo "Warning: sprite_converter.py not found, using existing sprites"; \
	fi
	@echo "Data extraction complete!"

# Build the Flipper Zero application using detected build tool
build:
	@echo "Building $(APP_ID) for Flipper Zero using $(notdir $(BUILD_TOOL))..."
	$(BUILD_TOOL) build

# Build and launch on connected Flipper Zero
launch: build
	@echo "Launching $(APP_ID) on Flipper Zero..."
	$(BUILD_TOOL) launch

# Install to connected Flipper Zero
install: build
	@echo "Installing $(APP_ID) to Flipper Zero..."
	$(BUILD_TOOL) install

# Debug the application
debug: build
	@echo "Starting debug session for $(APP_ID)..."
	$(BUILD_TOOL) debug

# Clean build artifacts
clean:
	@echo "Cleaning build artifacts..."
	$(BUILD_TOOL) clean
	@echo "Removing generated data files..."
	@rm -f pokemon_yellow_data.c moves_yellow_data.c
	@rm -rf sprites/*.xbm sprites/pokemon_sprites.h

# Development helpers
format:
	@echo "Formatting code..."
	@if command -v clang-format >/dev/null 2>&1; then \
		find . -name "*.c" -o -name "*.h" | grep -v build | xargs clang-format -i; \
		echo "Code formatted successfully"; \
	else \
		echo "clang-format not found, skipping formatting"; \
	fi

lint:
	@echo "Running static analysis..."
	@if command -v cppcheck >/dev/null 2>&1; then \
		cppcheck --enable=all --std=c99 --suppress=missingIncludeSystem *.c; \
	else \
		echo "cppcheck not found, skipping lint"; \
	fi

# Flipper Zero specific targets
flash: install
	@echo "Application installed and ready to run"

update: clean all install
	@echo "Full rebuild and install complete"

# Show project info
info:
	@echo "Pokemon Yellow+ Enhanced Game for Flipper Zero"
	@echo "=============================================="
	@echo "App ID: $(APP_ID)"
	@echo "Project Directory: $(PROJECT_DIR)"
	@echo "Build Tool: $(BUILD_TOOL)"
	@echo ""
	@echo "Source Files:"
	@find . -name "*.c" -not -path "./build/*" | sed 's/^/  /'
	@echo ""
	@echo "Header Files:"
	@find . -name "*.h" -not -path "./build/*" | sed 's/^/  /'
	@echo ""
	@echo "Available targets:"
	@echo "  all         - Extract data and build"
	@echo "  extract_data - Extract Pokemon data from pokeyellow"
	@echo "  build       - Build Flipper Zero application"
	@echo "  launch      - Build and launch on connected Flipper"
	@echo "  install     - Install to connected Flipper Zero"
	@echo "  debug       - Start debug session"
	@echo "  clean       - Clean build artifacts"
	@echo "  format      - Format source code"
	@echo "  lint        - Run static analysis"
	@echo "  flash       - Alias for install"
	@echo "  update      - Clean rebuild and install"
	@echo "  info        - Show this information"

# Help target
help: info

# Check if we're in a valid Flipper project
check:
	@if [ ! -f "application.fam" ]; then \
		echo "Error: application.fam not found. Are you in a Flipper Zero project directory?"; \
		exit 1; \
	fi
	@echo "✓ Valid Flipper Zero project structure detected"
	@echo "✓ Build tool: $(BUILD_TOOL)"
