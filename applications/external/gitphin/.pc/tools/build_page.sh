#!/bin/sh
# Usage: pc/tools/build_page.sh  - writes flipper_app/files/index.html.gz from index.html (gzip -9 -n); run before ufbt launch.
cd "$(dirname "$0")/../../flipper_app/files" && gzip -9 -n -c index.html > index.html.gz
