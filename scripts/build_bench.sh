#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

mkdir -p build
cd build
cmake ..
make orderbook_perf_harness

if [ $? -eq 0 ]; then
    echo "------------------------------------------------"
    echo "Build successful: $PROJECT_ROOT/build/orderbook_perf_harness"
    echo "------------------------------------------------"
else
    echo "Build failed."
    exit 1
fi