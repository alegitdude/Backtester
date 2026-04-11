#!/bin/bash

# 1. Location-agnostic root finding
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# 2. Paths
BINARY="$PROJECT_ROOT/build/reader_perf_harness"
DATA_FILE="../test/test_data/ES-glbx-20251105.mbo.csv.zst" 

# Define exactly where the perf data will live
OUTPUT_DIR="$PROJECT_ROOT/benchmarks/reader_perf_data"
PERF_DATA_FILE="$OUTPUT_DIR/perf.data"

# 3. Sanity checks
if [ ! -f "$BINARY" ]; then
    echo "ERROR: Binary not found at $BINARY"
    exit 1
fi

if [ -z "$DATA_FILE" ]; then
    echo "ERROR: No data file provided."
    echo "Usage: ./scripts/profile_reader.sh ../test/test_data/ES-glbx-20251105.mbo.csv.zst"
    exit 1
fi

# 4. Create the target directory if it doesn't exist
mkdir -p "$OUTPUT_DIR"

echo "Starting profile on: $BINARY"
echo "Saving raw perf data to: $PERF_DATA_FILE"

# 5. Run perf
# Notice the -o flag tells perf exactly where to save the file
sudo perf record -F 99 -g -o "$PERF_DATA_FILE" -- "$BINARY" "$DATA_FILE"

# 6. Fix permissions on the entire output folder so you can read it without sudo
sudo chown -R $USER:$USER "$OUTPUT_DIR"

echo "Profiling complete. Run ./scripts/generate_flame.sh to see results."