#!/usr/bin/env bash
# ./backtester_profile.sh <config> <runs> <single|threaded>
# End-to-end benchmark of the full Backtester binary.
#
# Measures:
#   - RunLoop wall-clock + throughput  (from the binary's own log line in ./logs)
#   - peak resident memory             (/usr/bin/time -v Maximum resident set size)
# over N runs, reports the median with min/max
#
# Run scripts/bench_env.sh first (fixed clocks, warm cache). Non-persistent.
set -euo pipefail

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

CONFIG="${1:-$PROJECT_ROOT/config/demo.json}"
RUNS="${2:-10}"

MODE="${3:-threaded}"
MODE="${MODE#--}"        # strip leading -- if present
case "$MODE" in
  single|threaded) ;;
  *) echo "mode must be 'single' or 'threaded', got '$MODE'" >&2; exit 1 ;;
esac

BINARY="$PROJECT_ROOT/build/Backtester"
BENCH_MD="$PROJECT_ROOT/docs/BENCHMARKS.md"
LOG_DIR="$PROJECT_ROOT/logs"

if [ "$MODE" = "single" ]; then
  CORES="${BENCH_CORES:-2}"
else
  CORES="${BENCH_CORES:-2,4}"
fi

if [ ! -x "$BINARY" ]; then
  echo "Binary not found: $BINARY  (build Release first)" >&2
  exit 1
fi
command -v /usr/bin/time >/dev/null || { echo "/usr/bin/time not found (apt install time)" >&2; exit 1; }

# --- Environment sanity (warn, don't fail) ---------------------------------
if [ -r /sys/devices/system/cpu/intel_pstate/no_turbo ] &&
   [ "$(cat /sys/devices/system/cpu/intel_pstate/no_turbo)" != "1" ]; then
  echo "  [warn] Turbo enabled; clocks will drift. Run bench_env.sh." >&2
fi

FIRST_CORE="${CORES%%,*}"
GOV_FILE="/sys/devices/system/cpu/cpu${FIRST_CORE}/cpufreq/scaling_governor"
if [ -r "$GOV_FILE" ] && [ "$(cat "$GOV_FILE")" != "performance" ]; then
  echo "  [warn] cpu${FIRST_CORE} governor is not 'performance'." >&2
fi

echo "== Full backtest benchmark =="
echo "  binary=$BINARY"
echo "  config=$CONFIG"
echo "  core=$CORES  runs=$RUNS"
echo

# ---------------------------------------------------------------------------
# Each run: run the binary, then read its throughput line from the newest file
# in ./logs, and peak RSS from /usr/bin/time (stderr).
# ---------------------------------------------------------------------------

secs=()
thru=()
rss_kb=()

TIME_OUT="$(mktemp)"
RUN_OUT="$(mktemp)"
trap 'rm -f "$TIME_OUT" "$RUN_OUT"' EXIT

for ((r = 1; r <= RUNS; r++)); do
  # Run; /usr/bin/time -v -> stderr (RSS). Binary's own logs go to ./logs/*.log
  taskset -c "$CORES" /usr/bin/time -v \
    "$BINARY" "$CONFIG" "$MODE" > "$RUN_OUT" 2> "$TIME_OUT" || {
      echo "run $r failed; see output:" >&2; cat "$TIME_OUT" >&2; exit 1; }

  # Peak RSS (KB). `|| true` so a missed match never aborts under set -e.
  rss="$(grep -F 'Maximum resident set size' "$TIME_OUT" | grep -oE '[0-9]+' | tail -1 || true)"

  # Newest log file written by this run.
  LOG_FILE="$(ls -t "$LOG_DIR"/*.log 2>/dev/null | head -1 || true)"

  # Throughput line: search the log file first, then the streams as fallback.
  line="$(grep -hE 'M evt/s' "$LOG_FILE" "$RUN_OUT" "$TIME_OUT" 2>/dev/null | tail -1 || true)"
  s="$(printf '%s' "$line" | grep -oE '[0-9]+\.[0-9]+s'  | head -1 | tr -d 's'  || true)"
  t="$(printf '%s' "$line" | grep -oE '[0-9]+\.[0-9]+ M' | head -1 | tr -d ' M' || true)"

  if [ -z "${s:-}" ] || [ -z "${t:-}" ]; then
    echo "  [warn] run $r: couldn't parse throughput line." >&2
    echo "         log file checked: ${LOG_FILE:-<none found>}" >&2
    echo "         line found: '${line:-<empty>}'" >&2
  fi

  secs+=("${s:-0}")
  thru+=("${t:-0}")
  rss_kb+=("${rss:-0}")
  printf "  run %2d/%d:  %ss   %s M evt/s   RSS %s MB\n" \
    "$r" "$RUNS" "${s:-?}" "${t:-?}" "$(awk -v k="${rss:-0}" 'BEGIN{printf "%.0f", k/1024}')"
done

# --- median / min / max helpers --------------------------------------------
median() {   # median; even count -> mean of middle two
  local sorted n mid
  mapfile -t sorted < <(printf '%s\n' "$@" | sort -n)
  n=${#sorted[@]}; mid=$((n/2))
  if (( n % 2 )); then echo "${sorted[$mid]}"
  else awk -v a="${sorted[$((mid-1))]}" -v b="${sorted[$mid]}" 'BEGIN{printf "%.3f", (a+b)/2}'
  fi
}
minv() { printf '%s\n' "$@" | sort -n | head -1; }
maxv() { printf '%s\n' "$@" | sort -n | tail -1; }

s_med="$(median "${secs[@]}")";  s_min="$(minv "${secs[@]}")";  s_max="$(maxv "${secs[@]}")"
t_med="$(median "${thru[@]}")"
rss_med_kb="$(median "${rss_kb[@]}")"
rss_med_mb="$(awk -v k="$rss_med_kb" 'BEGIN{printf "%.0f", k/1024}')"

COMMIT="$(git -C "$PROJECT_ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"
CPU="$(lscpu | awk -F: '/Model name/{gsub(/^ +/,"",$2); print $2; exit}')"
DATE="$(date -Iseconds)"

echo
echo "== Median over $RUNS runs =="
printf "  RunLoop:     %s s   (min %s / max %s)\n" "$s_med" "$s_min" "$s_max"
printf "  Throughput:  %s M evt/s\n" "$t_med"
printf "  Peak RSS:    %s MB\n" "$rss_med_mb"