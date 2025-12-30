#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_NUMBER_FILE="$SCRIPT_DIR/build_number.txt"
FIRMWARE_DIR="/Users/thomaskekeisen/Projects/flipperzero-firmware-unleashed/applications_user/tonuino_writer"

if [ -f "$BUILD_NUMBER_FILE" ]; then
    BUILD_NUMBER=$(cat "$BUILD_NUMBER_FILE")
    BUILD_NUMBER=$((BUILD_NUMBER + 1))
else
    BUILD_NUMBER=10
fi

echo "$BUILD_NUMBER" > "$BUILD_NUMBER_FILE"

sed -i.bak "s/#define APP_BUILD_NUMBER [0-9]*/#define APP_BUILD_NUMBER $BUILD_NUMBER/" "$SCRIPT_DIR/application.h"
rm -f "$SCRIPT_DIR/application.h.bak"

echo "Syncing files to firmware directory..."
cp "$SCRIPT_DIR/application.c" "$FIRMWARE_DIR/application.c"
cp "$SCRIPT_DIR/application.h" "$FIRMWARE_DIR/application.h"
cp "$SCRIPT_DIR/application.fam" "$FIRMWARE_DIR/application.fam"
echo "✓ Files synced"

cd /Users/thomaskekeisen/Projects/flipperzero-firmware-unleashed && ./fbt -c fap_tonuino_writer && ./fbt fap_tonuino_writer 2>&1 | tail -30

FAP_FILE="/Users/thomaskekeisen/Projects/flipperzero-firmware-unleashed/build/f7-firmware-D/.extapps/tonuino_writer.fap"

if [ -f "$FAP_FILE" ]; then
    echo ""
    echo "Installing app to Flipper Zero..."
    cd /Users/thomaskekeisen/Projects/flipperzero-firmware-unleashed && ./fbt launch APPSRC=tonuino_writer 2>&1 | tail -10
    if [ $? -eq 0 ]; then
        echo "✓ Successfully installed app to Flipper Zero"
    else
        echo "✗ Failed to install app (make sure Flipper Zero is connected)"
        echo "FAP file location: $FAP_FILE"
        echo "You can manually copy it to: /ext/apps/Tools/ on your Flipper Zero"
    fi
fi
