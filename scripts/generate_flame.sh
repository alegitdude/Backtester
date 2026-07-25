#!/bin/bash

HARNESS="${1:?usage: generate_flame.sh <reader_perf_harness|orderbook_perf_harness>}"
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

DATA_DIR="$PROJECT_ROOT/benchmarks/$HARNESS"
PERF_DATA_FILE="$DATA_DIR/perf.data"
SVG_OUTPUT="$DATA_DIR/reader_ob_flame.svg"

if [ ! -f "$PERF_DATA_FILE" ]; then
    echo "ERROR: No perf.data found at $PERF_DATA_FILE"
    echo "Make sure you ran the profiling script first."
    exit 1
fi

echo "Generating Flame Graph from $PERF_DATA_FILE..."

# Generate the graph
perf script -i "$PERF_DATA_FILE" --no-inline | ./FlameGraph/stackcollapse-perf.pl | ./FlameGraph/flamegraph.pl > "$SVG_OUTPUT"

echo "------------------------------------------------"
echo "Flame Graph generated successfully!"
echo "$SVG_OUTPUT"
echo "------------------------------------------------"