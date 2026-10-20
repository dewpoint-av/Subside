#!/usr/bin/env bash
#
# Subside - macOS installer package (.pkg).
# Installs to /Library/Audio/Plug-Ins/{VST3,Components,CLAP}.
#
set -euo pipefail
cd "$(dirname "$0")"

VERSION="1.0.0"
PAYLOAD="build/pkg-payload"
OUT="installer/output/Subside-${VERSION}-mac.pkg"

for b in dist/Subside.vst3 dist/Subside.component dist/Subside.clap; do
    if [ ! -d "$b" ]; then
        echo "ERROR: $b not found. Run ./build_mac.sh first." >&2
        exit 1
    fi
done

echo "Preparing package payload..."
rm -rf "$PAYLOAD"
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/VST3"
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/Components"
mkdir -p "$PAYLOAD/Library/Audio/Plug-Ins/CLAP"
cp -R dist/Subside.vst3      "$PAYLOAD/Library/Audio/Plug-Ins/VST3/"
cp -R dist/Subside.component "$PAYLOAD/Library/Audio/Plug-Ins/Components/"
cp -R dist/Subside.clap      "$PAYLOAD/Library/Audio/Plug-Ins/CLAP/"

mkdir -p installer/output
rm -f "$OUT"

echo "Building installer package..."
pkgbuild --root "$PAYLOAD" \
    --identifier "com.dewpoint.Subside" \
    --version "$VERSION" \
    --install-location "/" \
    "$OUT"

echo
echo "Created: $OUT"
