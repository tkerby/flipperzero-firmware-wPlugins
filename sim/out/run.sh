#!/bin/sh
# Render every page to sim/out/*.pgm. Convert with: python3 -c "..." (see README)
set -eu
cd "$(dirname "$0")/../.."
./sim/out/rfsim sim/out
printf 'scenes: '; ls sim/out/*.pgm | wc -l
