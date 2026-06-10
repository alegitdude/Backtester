# scripts/profile.sh <harness-name> [config]
HARNESS="${1:?usage: profile.sh <reader_perf_harness|orderbook_perf_harness> [config]}"
CONFIG="${2:-$PROJECT_ROOT/config/demo.json}"
BINARY="$PROJECT_ROOT/build/$HARNESS"
OUTPUT_DIR="$PROJECT_ROOT/benchmarks/$HARNESS"      # per-harness, so they don't clobber
sudo perf record -F 99 -g -o "$OUTPUT_DIR/perf.data" -- "$BINARY" "$CONFIG"