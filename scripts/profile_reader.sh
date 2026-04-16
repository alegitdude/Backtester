#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

BINARY="$PROJECT_ROOT/build/orderbook_perf_harness"
DATA_FILE="../test/test_data/ES-glbx-20251105.mbo.csv.zst" 

OUTPUT_DIR="$PROJECT_ROOT/benchmarks/orderbook_perf_harness"
PERF_DATA_FILE="$OUTPUT_DIR/perf.data"

if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    exit 1
fi

if [ -z "$DATA_FILE" ]; then
    echo "ERROR: No data file provided."
    echo "Usage: ./scripts/profile_reader.sh $DATA_FILE"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

echo "Starting profile on: $BINARY"
echo "Saving raw perf data to: $PERF_DATA_FILE"

# Run perf
sudo perf record -F 99 -g -o "$PERF_DATA_FILE" -- "$BINARY" "$DATA_FILE"

# Fix permissions on the entire output folder so you can read it without sudo
sudo chown -R $USER:$USER "$OUTPUT_DIR"

echo "Profiling complete. Run ./scripts/generate_flame.sh to see results."