#!/usr/bin/env bash
# scripts/profile.sh <harness-name> [config]
#
# Runs three perf captures under a pinned, fixed-clock environment:
#   1. perf record          -> benchmarks/<harness>/perf.data   (for flame graphs, generated separately)
#   2. perf stat (group A)  -> time, cycles, instructions       (exact, no multiplexing)
#   3. perf stat (group B)  -> branch + cache-hierarchy events  (exact, no multiplexing)
# Prints raw counts, elapsed/task-clock, computed ratios (IPC, branch-miss%),
# and an interpreted cache-hierarchy breakdown (L1 miss%, LLC miss%, and how
# much of all loads actually reach DRAM), all from the same runs, to
# benchmarks/<harness>/perf_stat_<timestamp>.txt
#
# Pair with scripts/bench_env.sh to set the fixed-clock state before running.
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

HARNESS="${1:?usage: profile.sh <reader_perf_harness|orderbook_perf_harness> [config]}"
CONFIG="${2:-$PROJECT_ROOT/config/demo.json}"
BINARY="$PROJECT_ROOT/build/$HARNESS"
OUTPUT_DIR="$PROJECT_ROOT/benchmarks/$HARNESS"

CORE="${BENCH_CORE:-2}"

TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
STAT_OUT="$OUTPUT_DIR/perf_stat_${TIMESTAMP}.txt"

mkdir -p "$OUTPUT_DIR"

if [ ! -x "$BINARY" ]; then
  echo "Binary not found: $BINARY  (run build_bench.sh first)" >&2
  exit 1
fi

# --- Environment sanity checks (warn, don't fail) --------------------------
warn() { echo "  [warn] $*" >&2; }

echo "== Environment =="
PARANOID="$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo unknown)"
if [ "$PARANOID" != "unknown" ] && [ "$PARANOID" -gt 1 ]; then
  warn "perf_event_paranoid=$PARANOID; perf may need sudo."
  warn "For unprivileged runs: echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid"
fi

if [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ]; then
  if [ "$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)" != "1" ]; then
    warn "Turbo is ENABLED (intel_pstate/no_turbo=0). Clocks will drift; run bench_env.sh."
  fi
fi

GOV_FILE="/sys/devices/system/cpu/cpu${CORE}/cpufreq/scaling_governor"
if [ -r "$GOV_FILE" ]; then
  GOV="$(cat "$GOV_FILE")"
  [ "$GOV" = "performance" ] || warn "cpu${CORE} governor is '$GOV', not 'performance'."
fi

echo "  harness=$HARNESS  core=$CORE  config=$CONFIG"
echo

# --- 1. perf record (for flame graph, generated separately) ----------------
echo "== perf record -> $OUTPUT_DIR/perf.data =="
taskset -c "$CORE" perf record -F 99 -g -o "$OUTPUT_DIR/perf.data" -- "$BINARY" "$CONFIG"
echo

# --- 2 & 3. perf stat, split into two runs to avoid PMU multiplexing -------
STAT_A="$OUTPUT_DIR/.stat_a.csv"
STAT_B="$OUTPUT_DIR/.stat_b.csv"

echo "== perf stat (group A: time, cycles, instructions) =="
taskset -c "$CORE" perf stat -x, \
  -e duration_time,task-clock,cycles,instructions \
  -o "$STAT_A" -- "$BINARY" "$CONFIG"

echo "== perf stat (group B: branches, cache hierarchy) =="
# Explicit L1/LLC events (NOT the generic cache-references/cache-misses alias,
# whose meaning varies by CPU and is not comparable across machines).
taskset -c "$CORE" perf stat -x, \
  -e branches,branch-misses,L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
  -o "$STAT_B" -- "$BINARY" "$CONFIG"
echo

# --- Print raw counts, ratios, and interpreted cache breakdown -------------
# perf -x, with -o writes comment lines ('#') then rows:
#   <count>,<unit>,<event>,<run-ns>,<pct>,...   ; event name is field 3.
# duration_time and task-clock are reported in nanoseconds.
{
  echo "perf stat summary  harness=$HARNESS  core=$CORE  $(date -Iseconds)"
  echo "config=$CONFIG"
  echo ""
  awk -F, '
    /^#/ {next}
    $3=="duration_time"         {dur=$1}
    $3=="task-clock"            {task=$1}
    $3=="cycles"                {c=$1}
    $3=="instructions"          {i=$1}
    $3=="branches"              {b=$1}
    $3=="branch-misses"         {bm=$1}
    $3=="L1-dcache-loads"       {l1=$1}
    $3=="L1-dcache-load-misses" {l1m=$1}
    $3=="LLC-loads"             {llc=$1}
    $3=="LLC-load-misses"       {llcm=$1}
    END{
      # ---- raw counts ----
      printf "%18d  cycles\n",                c
      printf "%18d  instructions\n",          i
      printf "%18d  branches\n",              b
      printf "%18d  branch-misses\n",         bm
      printf "%18d  L1-dcache-loads\n",       l1
      printf "%18d  L1-dcache-load-misses\n", l1m
      printf "%18d  LLC-loads\n",             llc
      printf "%18d  LLC-load-misses\n",       llcm
      print  ""
      if (dur>0)  printf "%18.9f  seconds time elapsed\n", dur/1e9
      if (task>0) printf "%18.9f  seconds task-clock (on-CPU)\n", task/1e9
      print  ""

      # ---- core ratios ----
      print "---- ratios ----"
      if (c>0) printf "IPC:          %.2f\n",   i/c
      if (b>0) printf "branch-miss:  %.2f%%\n", bm/b*100
      print  ""

      # ---- interpreted cache hierarchy ----
      print "---- cache hierarchy ----"
      if (l1>0)
        printf "L1-dcache miss:  %.2f%%   (%d / %d loads)\n", l1m/l1*100, l1m, l1
      if (llc>0 && l1>0) {
        printf "LLC-load miss:   %.2f%%   (%d / %d LLC refs)\n", llcm/llc*100, llcm, llc
        printf "  LLC refs are %.3f%% of L1 loads; ~%.3f%% of all loads reach DRAM.\n", \
               llc/l1*100, llcm/l1*100
        if (llc/l1 < 0.02)
          print "  (LLC ref count is tiny, so a high LLC-miss % is expected and healthy.)"
        else
          print "  (LLC refs are a sizeable fraction of loads -- real memory traffic; investigate.)"
      }
    }' "$STAT_A" "$STAT_B"
} | tee "$STAT_OUT"

rm -f "$STAT_A" "$STAT_B"

echo
echo "Saved: $STAT_OUT"
echo "Flame graph: generate separately from $OUTPUT_DIR/perf.data"