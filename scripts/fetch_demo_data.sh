#!/usr/bin/env bash
set -euo pipefail

DEST_DIR="$(dirname "$0")/../test/test_data"
RELEASE_URL="https://github.com/alegitdude/Backtester/releases/download/demo-data"

mkdir -p "$DEST_DIR"

# Pick whichever downloader is available
if command -v curl >/dev/null 2>&1; then
    DOWNLOAD() { curl -L --fail --progress-bar -o "$1" "$2"; }
elif command -v wget >/dev/null 2>&1; then
    DOWNLOAD() { wget --show-progress -O "$1" "$2"; }
else
    echo "Error: neither curl nor wget is installed." >&2
    echo "Install one with: sudo apt install curl" >&2
    exit 1
fi

FILES=(
    "ES-glbx-20251105.mbo.csv.zst"
    "ES-glbx-20251105.mbp-10.csv.zst"
)

for f in "${FILES[@]}"; do
    if [ -f "$DEST_DIR/$f" ]; then
        echo "Already have $f, skipping."
        continue
    fi
    echo "Downloading $f..."
    DOWNLOAD "$DEST_DIR/$f" "$RELEASE_URL/$f"
done