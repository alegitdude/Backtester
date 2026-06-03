#!/usr/bin/env bash
# Downloads the demo MBO data from the GitHub release.
set -euo pipefail

DEST_DIR="$(dirname "$0")/../test/test_data"
RELEASE_URL="https://github.com/YOURUSER/YOURREPO/releases/download/demo-data"

mkdir -p "$DEST_DIR"

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
    curl -L --fail --progress-bar -o "$DEST_DIR/$f" "$RELEASE_URL/$f"
done

echo "Demo data ready in $DEST_DIR"