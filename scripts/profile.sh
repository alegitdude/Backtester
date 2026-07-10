# scripts/profile.sh <harness-name> [config]
#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

HARNESS="${1:?usage: profile.sh <reader_perf_harness|orderbook_perf_harness> [config]}"
CONFIG="${2:-$PROJECT_ROOT/config/demo.json}"
BINARY="$PROJECT_ROOT/build/$HARNESS"
OUTPUT_DIR="$PROJECT_ROOT/benchmarks/$HARNESS"

mkdir -p "$OUTPUT_DIR"

if [ ! -x "$BINARY" ]; then
  echo "Binary not found: $BINARY  (run build_bench.sh first)" >&2
  exit 1
fi

sudo perf record -F 99 -g -o "$OUTPUT_DIR/perf.data" -- "$BINARY" "$CONFIG"