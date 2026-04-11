#!/bin/bash
echo "Here"
# 1. Location-agnostic root finding
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# 2. Paths pointing to the new benchmark data folder
DATA_DIR="$PROJECT_ROOT/benchmarks/reader_perf_data"
PERF_DATA_FILE="$DATA_DIR/perf.data"
SVG_OUTPUT="$DATA_DIR/reader_flame.svg"

# 3. Sanity check
if [ ! -f "$PERF_DATA_FILE" ]; then
    echo "ERROR: No perf.data found at $PERF_DATA_FILE"
    echo "Make sure you ran the profiling script first."
    exit 1
fi

echo "Generating Flame Graph from $PERF_DATA_FILE..."

# 4. Generate the graph
# Notice the -i flag tells perf script where to find the input file
perf script -i "$PERF_DATA_FILE" | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > "$SVG_OUTPUT"

echo "------------------------------------------------"
echo "Flame Graph generated successfully!"
echo "Open this file in your browser: $SVG_OUTPUT"
echo "------------------------------------------------"