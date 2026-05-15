#!/usr/bin/env python3
"""
Oracle barrier test for MovAvgCross.
 
Reads two things:
  1. The backtester's spdlog output, to extract each (entry_ts, entry_price, side)
     and pair it with the next exit so we know which entry actually closed.
  2. The raw MBO trade tape (.csv.zst), filtered to action == 'T' rows.
 
For each entry, scans the trade tape forward from entry_ts and reports which
barrier — take-profit (kTakeProfitPoints in the strategy's direction) or
stop-loss (kStopLossPoints against) — is crossed FIRST by a trade print.
 
If the simulator reports 23 winners and 0 losers but the oracle says ~half the
entries should have hit the stop first, the bug is in the executor's fill
logic. If the oracle agrees 23/23 are wins, the data genuinely supported every
position and the simulator is correct (or biased in a different way).
 
Usage:
    pip install zstandard
    python oracle_barrier_test.py <log_file> <mbo.csv.zst> [--instr 294973]
 
Defaults to ES futures contract 294973 and the strategy's hardcoded 5/7 point
barriers; override with --tp-points / --sl-points / --instr if needed.
"""
import argparse
import io
import re
import sys
from dataclasses import dataclass
from typing import Optional
 
try:
    import zstandard as zstd
except ImportError:
    sys.stderr.write("Install zstandard: pip install zstandard\n")
    sys.exit(1)
 
 
# Fixed-point scale used throughout the C++ codebase: 1.0 == 1e9 ticks.
ONE_POINT = 1_000_000_000
 
 
@dataclass
class Entry:
    ts: int           # epoch ns when the entry fill occurred
    price: int        # fill price in 1e-9 fixed point
    side: str         # 'long' or 'short'
    order_id: int
 
 
# -----------------------------------------------------------------------------
# Log parsing
# -----------------------------------------------------------------------------
 
# Matches "FillEvent emitted — order_id=2 instr=294973 side=1 price=6818500000000 qty=1 ts=1762..."
# side: 0 == bid (buy), 1 == ask (sell). qty=0 means a synthetic cancel-fill from
# CancelAllPendingOrders at EOD and should be ignored.
FILL_RE = re.compile(
    r"FillEvent emitted .* order_id=(?P<oid>-?\d+) "
    r"instr=(?P<instr>\d+) side=(?P<side>[01]) "
    r"price=(?P<price>\d+) qty=(?P<qty>\d+) ts=(?P<ts>\d+)"
)
 
 
def parse_fills(log_path: str, instr_id: int) -> list[Entry]:
    """
    Walks the log in order. Tracks a running signed position; an entry is any
    fill that moves position away from zero (open or flip). Returns only the
    entries, not the exits.
 
    On a flip (e.g. long 1 -> sell 2 -> short 1), the second contract of that
    sell is the new entry; the recorded entry price is the fill price of that
    flipping order (which is what the strategy's avg_entry_price would also
    record per the OnFill logic).
    """
    fills = []
    with open(log_path) as f:
        for line in f:
            m = FILL_RE.search(line)
            if not m:
                continue
            if int(m["instr"]) != instr_id:
                continue
            if int(m["qty"]) == 0:
                continue  # synthetic EOD cancel
            fills.append({
                "oid": int(m["oid"]),
                "side": "buy" if m["side"] == "0" else "sell",
                "price": int(m["price"]),
                "qty": int(m["qty"]),
                "ts": int(m["ts"]),
            })
 
    entries: list[Entry] = []
    pos = 0
    for fill in fills:
        delta = fill["qty"] if fill["side"] == "buy" else -fill["qty"]
        new_pos = pos + delta
 
        # An entry: we were flat and now we're not, OR we crossed zero.
        opens_fresh = (pos == 0 and new_pos != 0)
        flips = (pos != 0 and new_pos != 0 and (pos > 0) != (new_pos > 0))
 
        if opens_fresh or flips:
            entries.append(Entry(
                ts=fill["ts"],
                price=fill["price"],
                side="long" if new_pos > 0 else "short",
                order_id=fill["oid"],
            ))
        pos = new_pos
 
    return entries
 
 
# -----------------------------------------------------------------------------
# Trade tape scanning
# -----------------------------------------------------------------------------
 
def iter_trades(mbo_path: str, instr_id: int):
    """
    Streams the MBO csv.zst, yielding (ts_event_ns, price_fp) for trades on the
    target instrument. Assumes Databento MBO schema; price column is decimal so
    we scale to fixed-point on the fly.
 
    Yielding tuples (not dicts) and skipping unrelated rows early matters — the
    full ES tape is ~16M rows.
    """
    dctx = zstd.ZstdDecompressor()
    with open(mbo_path, "rb") as fh:
        with dctx.stream_reader(fh) as stream:
            text = io.TextIOWrapper(stream, encoding="utf-8")
            header = text.readline().rstrip("\r\n").split(",")
            idx_ts = header.index("ts_event")
            idx_instr = header.index("instrument_id")
            idx_action = header.index("action")
            idx_price = header.index("price")
 
            instr_str = str(instr_id)
 
            for row in text:
                # Cheap pre-filter: trades are 'T' in the action column. Splitting
                # is unavoidable but most rows aren't trades; we could substring-
                # check first but the pattern is column-positional so just split.
                parts = row.rstrip("\r\n").split(",")
                if len(parts) <= idx_price:
                    continue
                if parts[idx_action] != "T":
                    continue
                if parts[idx_instr] != instr_str:
                    continue
                # Databento ts_event is unix nanos (integer string) in this tape.
                # If your tape uses ISO timestamps, swap this for the parser.
                try:
                    ts = int(parts[idx_ts])
                except ValueError:
                    # ISO timestamp fallback
                    from datetime import datetime, timezone
                    dt = datetime.fromisoformat(parts[idx_ts].replace("Z", "+00:00"))
                    ts = int(dt.replace(tzinfo=timezone.utc).timestamp() * 1e9) + \
                         int(parts[idx_ts].split(".")[1].rstrip("Z")[:9].ljust(9, "0")) % 1_000_000_000
 
                # Price as fixed point
                price_fp = int(parts[idx_price])
                yield ts, price_fp
 
 
# -----------------------------------------------------------------------------
# Oracle decision per entry
# -----------------------------------------------------------------------------
 
@dataclass
class OracleResult:
    entry: Entry
    outcome: str       # 'tp', 'sl', 'no_decision'
    barrier_ts: Optional[int]
    barrier_price: Optional[int]
    trades_scanned: int
 
 
def decide(entry: Entry, trades: list[tuple[int, int]],
           tp_points: int, sl_points: int) -> OracleResult:
    """
    For one entry, walk `trades` (sorted by ts) and find the first trade whose
    price crosses TP or SL relative to the entry price+side. Returns whichever
    barrier was hit first.
    """
    tp_dist = tp_points * ONE_POINT
    sl_dist = sl_points * ONE_POINT
 
    if entry.side == "long":
        tp_level = entry.price + tp_dist
        sl_level = entry.price - sl_dist
        hit_tp = lambda px: px >= tp_level
        hit_sl = lambda px: px <= sl_level
    else:
        tp_level = entry.price - tp_dist
        sl_level = entry.price + sl_dist
        hit_tp = lambda px: px <= tp_level
        hit_sl = lambda px: px >= sl_level
 
    scanned = 0
    # Skip trades at or before entry_ts. The trade that filled us isn't a
    # candidate for the exit barrier.
    for ts, px in trades:
        if ts <= entry.ts:
            continue
        scanned += 1
        if hit_tp(px):
            return OracleResult(entry, "tp", ts, px, scanned)
        if hit_sl(px):
            return OracleResult(entry, "sl", ts, px, scanned)
    return OracleResult(entry, "no_decision", None, None, scanned)
 
 
# -----------------------------------------------------------------------------
# Main
# -----------------------------------------------------------------------------
 
def fmt_price(fp: int) -> str:
    return f"{fp / ONE_POINT:.2f}"
 
 
def fmt_ts(ns: int) -> str:
    sec = ns // 1_000_000_000
    rem = ns %  1_000_000_000
    from datetime import datetime, timezone
    return datetime.fromtimestamp(sec, tz=timezone.utc).strftime(
        "%Y-%m-%d %H:%M:%S") + f".{rem:09d}"
 
 
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log_file")
    ap.add_argument("mbo_file")
    ap.add_argument("--instr", type=int, default=294973)
    ap.add_argument("--tp-points", type=int, default=5,
                    help="Take-profit distance in index points (default 5)")
    ap.add_argument("--sl-points", type=int, default=7,
                    help="Stop-loss distance in index points (default 7)")
    args = ap.parse_args()
 
    print(f"Parsing entries from {args.log_file} ...", file=sys.stderr)
    entries = parse_fills(args.log_file, args.instr)
    print(f"  found {len(entries)} entries", file=sys.stderr)
 
    print(f"Loading trades from {args.mbo_file} ...", file=sys.stderr)
    # Materialize to a list once; cheap compared to per-entry scans.
    trades = list(iter_trades(args.mbo_file, args.instr))
    print(f"  loaded {len(trades)} trades for instrument {args.instr}",
          file=sys.stderr)
 
    results = [decide(e, trades, args.tp_points, args.sl_points) for e in entries]
 
    # Per-entry table
    print()
    print(f"{'#':>3}  {'side':>5}  {'entry_ts':<29}  {'entry_px':>10}  "
          f"{'outcome':>11}  {'barrier_px':>10}  {'scanned':>8}")
    print("-" * 95)
    for i, r in enumerate(results, 1):
        bp = fmt_price(r.barrier_price) if r.barrier_price else "—"
        print(f"{i:>3}  {r.entry.side:>5}  {fmt_ts(r.entry.ts):<29}  "
              f"{fmt_price(r.entry.price):>10}  {r.outcome:>11}  "
              f"{bp:>10}  {r.trades_scanned:>8}")
 
    # Summary
    n_tp = sum(1 for r in results if r.outcome == "tp")
    n_sl = sum(1 for r in results if r.outcome == "sl")
    n_nd = sum(1 for r in results if r.outcome == "no_decision")
    total = len(results)
    print()
    print(f"Oracle result over {total} entries (TP={args.tp_points}pt, "
          f"SL={args.sl_points}pt):")
    print(f"  TP hit first:  {n_tp:>3}  ({100 * n_tp / total:.1f}%)" if total else "")
    print(f"  SL hit first:  {n_sl:>3}  ({100 * n_sl / total:.1f}%)" if total else "")
    print(f"  no decision:   {n_nd:>3}  ({100 * n_nd / total:.1f}%)" if total else "")
    print()
    print("Interpretation:")
    print("  - If TP rate is near 100%, the historical tape supported the wins")
    print("    and the simulator's 23:0 may be correct; investigate entry timing.")
    print("  - If TP rate is well below the simulator's reported rate, the")
    print("    executor is mis-filling and the bug is real.")
 
 
if __name__ == "__main__":
    main()
