#!/usr/bin/env bash
set -euo pipefail
# Non-persistent — run once per session from repo root.
sudo -v   # prompt for password up front, cache it
sudo cpupower -c all frequency-set -g performance
echo 1  | sudo tee /sys/devices/system/cpu/intel_pstate/no_turbo >/dev/null
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid >/dev/null
echo 0  | sudo tee /proc/sys/kernel/kptr_restrict >/dev/null
cat "$(git rev-parse --show-toplevel)/test/test_data/ES-glbx-20251105.mbo.csv.zst" > /dev/null

echo 0  | sudo tee /proc/sys/kernel/kptr_restrict
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid
echo "bench env ready: performance governor, turbo off, perf unrestricted, cache warm"