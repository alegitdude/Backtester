#!/bin/bash

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# 2. Build via CMake
mkdir -p build
cd build
cmake ..
make reader_perf_harness

if [ $? -eq 0 ]; then
    echo "------------------------------------------------"
    echo "Build successful: $PROJECT_ROOT/build/reader_perf_harness"
    echo "------------------------------------------------"
else
    echo "Build failed."
    exit 1
fi