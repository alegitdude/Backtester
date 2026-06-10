#!/usr/bin/env bash
set -euo pipefail
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

CONFIG="$PROJECT_ROOT/config/demo.json"
DATA="$PROJECT_ROOT/test/test_data/ES-glbx-20251105.mbo.csv.zst"
RUNS="${RUNS:-2}"   # override with: RUNS=5 ./scripts/run_benchmarks.sh

if [ ! -f "$DATA" ]; then
  echo "Demo data not found at: $DATA"
  echo "Fetch it first:  ./scripts/fetch_demo_data.sh"
  exit 1
fi

# Build the harnesses in Release (idempotent — reuses build/).
"$SCRIPT_DIR/build_bench.sh"

run() {
  local name="$1"
  echo
  echo "=============================================================="
  echo " $name  ($RUNS runs)"
  echo "=============================================================="
  for i in $(seq 1 "$RUNS"); do
    echo "--- run $i ---"
    "$PROJECT_ROOT/build/$name" "$CONFIG"
  done
}

run reader_perf_harness
run orderbook_perf_harness