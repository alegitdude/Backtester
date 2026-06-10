#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

# Default to both harnesses; override by passing target names.
TARGETS=("$@")
if [ ${#TARGETS[@]} -eq 0 ]; then
  TARGETS=(reader_perf_harness orderbook_perf_harness)
fi

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j --target "${TARGETS[@]}"

echo "Built (Release): ${TARGETS[*]} -> $PROJECT_ROOT/build/"