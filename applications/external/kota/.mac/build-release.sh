#!/bin/zsh
# Builds a universal (arm64 + x86_64) kota binary and the tarball the Homebrew formula downloads.
# Per-arch native builds + lipo: Swift 6.4's default build system fails under Command Line Tools.
set -euo pipefail
cd "${0:A:h}"
VERSION=$(sed -n 's/^let version = "\(.*\)"/\1/p' Sources/kota/main.swift)
for arch in arm64 x86_64; do
  swift build -c release --build-system native --triple $arch-apple-macosx13.0
done
mkdir -p dist
lipo -create .build/arm64-apple-macosx/release/kota .build/x86_64-apple-macosx/release/kota -output dist/kota
codesign -s - -f -i dev.cobanov.kota dist/kota
dist/kota --self-check
tar -C dist -czf dist/kota-$VERSION-macos.tar.gz kota
shasum -a 256 dist/kota-$VERSION-macos.tar.gz
