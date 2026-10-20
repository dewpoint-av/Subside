#!/usr/bin/env bash
#
# Subside - macOS drag-and-drop installer (.dmg).
#
set -euo pipefail
cd "$(dirname "$0")"

VERSION="1.0.0"
STAGE="build/dmg-root"
OUT="installer/output/Subside-${VERSION}-mac.dmg"

for b in dist/Subside.vst3 dist/Subside.component dist/Subside.clap; do
    if [ ! -d "$b" ]; then
        echo "ERROR: $b not found. Run ./build_mac.sh first." >&2
        exit 1
    fi
done

echo "Preparing .dmg staging area..."
rm -rf "$STAGE"
mkdir -p "$STAGE"
cp -R dist/Subside.vst3      "$STAGE/"
cp -R dist/Subside.component "$STAGE/"
cp -R dist/Subside.clap      "$STAGE/"

ln -sfn "/Library/Audio/Plug-Ins" "$STAGE/Audio Plug-Ins"

cat > "$STAGE/README.txt" <<'EOF'
Subside - Sidechain Ducker
--------------------------

Drag each bundle into the matching folder inside "Audio Plug-Ins":

    Subside.vst3        ->  VST3/
    Subside.component   ->  Components/
    Subside.clap        ->  CLAP/

Then rescan your plug-ins in the DAW.
EOF

mkdir -p installer/output
rm -f "$OUT"

echo "Creating disk image..."
hdiutil create -volname "Subside" -srcfolder "$STAGE" -ov -format UDZO "$OUT"

echo
echo "Created: $OUT"
