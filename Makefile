# Hunter Flipper Makefile
# Provides convenient targets for building and testing the game

# Path to ufbt virtual environment
UFBT = ~/ufbt-env/bin/ufbt

.PHONY: all build clean launch debug help install

# Default target
all: build

# Build the game
build:
	$(UFBT)

# Clean build artifacts
clean:
	$(UFBT) clean

# Build and launch on connected Flipper Zero
launch:
	$(UFBT) launch

# Build debug version
debug:
	$(UFBT) COMPACT=0

# Install to connected Flipper Zero without launching
install:
	$(UFBT) install

# Show build information
info:
	$(UFBT) info

# Format code (if available)
format:
	$(UFBT) format

# Lint code (if available) 
lint:
	$(UFBT) lint

# Show help
help:
	@echo "Hunter Flipper Build Targets:"
	@echo "  build   - Build the game (default)"
	@echo "  clean   - Clean build artifacts"
	@echo "  launch  - Build and launch on connected Flipper Zero"
	@echo "  debug   - Build debug version with symbols"
	@echo "  install - Install to Flipper Zero without launching"
	@echo "  info    - Show build information"
	@echo "  format  - Format source code"
	@echo "  lint    - Lint source code"
	@echo "  help    - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - ufbt installed in ~/ufbt-env/"
	@echo "  - Flipper Zero connected via USB (for launch/install)"
	@echo ""
	@echo "Examples:"
	@echo "  make build"
	@echo "  make launch"
	@echo "  make clean"