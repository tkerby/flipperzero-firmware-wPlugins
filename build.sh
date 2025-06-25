#!/bin/bash
# Build script for Pokemon Yellow+ Flipper Zero app

set -e

echo "Pokemon Yellow+ Build Script"
echo "============================"

# Check if we're in the right directory
if [ ! -f "application.fam" ]; then
    echo "Error: application.fam not found. Please run from the project directory."
    exit 1
fi

# Function to detect build tool
detect_build_tool() {
    if command -v ufbt >/dev/null 2>&1; then
        echo "ufbt"
    elif command -v fbt >/dev/null 2>&1; then
        echo "fbt"
    else
        echo "none"
    fi
}

BUILD_TOOL=$(detect_build_tool)

if [ "$BUILD_TOOL" = "none" ]; then
    echo "Error: Neither ufbt nor fbt found."
    echo "Please install the Flipper Zero SDK:"
    echo "  - For ufbt: pip install ufbt"
    echo "  - For full SDK: git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git"
    exit 1
fi

echo "Using build tool: $BUILD_TOOL"

# Extract Pokemon data if extractors are available
echo ""
echo "Step 1: Extracting Pokemon data..."
if [ -f "../pokemon_data_extractor.py" ]; then
    python3 ../pokemon_data_extractor.py
    echo "✓ Pokemon data extracted"
else
    echo "⚠ pokemon_data_extractor.py not found, using existing data"
fi

if [ -f "../sprite_converter.py" ]; then
    python3 ../sprite_converter.py
    echo "✓ Sprites converted"
else
    echo "⚠ sprite_converter.py not found, using existing sprites"
fi

# Build the application
echo ""
echo "Step 2: Building application..."
$BUILD_TOOL build

if [ $? -eq 0 ]; then
    echo "✓ Build successful!"
    echo ""
    echo "Next steps:"
    echo "  - To install: $BUILD_TOOL install"
    echo "  - To launch: $BUILD_TOOL launch"
    echo "  - To debug: $BUILD_TOOL debug"
else
    echo "✗ Build failed!"
    exit 1
fi
