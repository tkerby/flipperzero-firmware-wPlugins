#!/bin/bash
set -euo pipefail

cd "$(dirname "$0")"

if ! python3 -m ufbt --help >/dev/null 2>&1; then
    echo "Installing ufbt..."
    python3 -m pip install --user --upgrade ufbt
fi

python3 -m ufbt

echo "Built: $(pwd)/dist/flipper_fire.fap"
