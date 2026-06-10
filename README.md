# Event-Driven Backtester for High-Frequency Market Data
 
A single-threaded, event-driven backtesting framework in C++17 for futures and equities strategies, built around Databento MBO (Market-By-Order) feeds. Reconstructs the full limit order book from order-level data, replays strategies against historical events, and produces per-trade and equity-curve reports.
 
**Throughput on the hot path: 15.9 M events/sec** on a single core processing 16.1M ES futures MBO messages (full venue book reconstruction). See [BENCHMARKS.md](./docs/BENCHMARKS.md) for the full optimization log.
 
**Correctness validated against Databento MBP-10:** every reconstructed top-10 snapshot is asserted equal to the published aggregated book at the same sequence number. See [Validation](#validation).
 

---
 
## Table of Contents
- [Highlights](#highlights)
- [Architecture](#architecture)
- [Performance](#performance)
- [Validation](#validation)
- [Quickstart](#quickstart)
- [Configuration](#configuration)
- [Modeling Assumptions](#modeling-assumptions)
- [Scope & Limitations](#scope--limitations)
- [Project Layout](#project-layout)
- [Benchmarking & Profiling](#benchmarking--profiling)
- [Analysis & Visualization](#analysis--visualization)
---
 
## Highlights
 
- **Order book reconstruction from MBO.** Per-publisher books aggregated into a consolidated instrument-level BBO. Handles add / modify / cancel / clear / trade actions with F_LAST flag-driven BBO cache updates.
- **Custom open-addressing hash table with backshift deletion** for order-ID lookup, replacing `std::unordered_map`. Drove the orderbook hot path from 11.3 → 15.9 M events/sec by collapsing cache-miss-heavy chained-bucket scans into linear probes over contiguous memory.
- **Sorted-vector price levels stored worst→best**, searched from the back. New levels at or near the top of book are found and inserted in O(1) amortized; deep-book activity pays the linear search cost. Reduced level-search overhead vs. a `std::map`-based design and eliminated tree-rebalance cache misses.
- **Shadow-book execution model** that tracks queue position from MBO depth at order submission and fills only when sufficient size has traded through ahead. Supports a configurable `TopOfBook` model as an optimistic-fill benchmark.
- **Latency-aware execution.** Strategy orders are timestamped at submission but only become eligible to fill after a configurable latency offset (`execution_latency_ms`), modeling round-trip wire time to the venue.
- **Streaming zstd CSV reader** chunked decompression with bounded memory. Neither the compressed nor decompressed file is ever fully resident; only the current line is allocated as a string during parsing.
- **Strategy registry with self-registering classes** via a `REGISTER_STRATEGY` macro — drop a `.cpp` into `src/strategy/user_strategies/`, no edits to manager or CMake required.
- **Per-instrument margin, tick size, and tick value** for futures contract PnL accounting. Position flip, partial close, and FIFO PnL handled correctly across long/short transitions (see `PortfolioManager_test.cpp`).
## Architecture
 
```
                       ┌──────────────────────────┐
   MBO.csv.zst ───────►│  DataReaderManager       │  streaming zstd + CSV parse
                       │  (per-source readers)    │
                       └──────────┬───────────────┘
                                  │  MarketByOrderEvent
                                  ▼
                       ┌──────────────────────────┐
                       │  EventQueue              │  min-heap by (ts, type)
                       │  (priority queue)        │
                       └──────────┬───────────────┘
                                  │
              ┌───────────────────┼───────────────────────────┐
              ▼                   ▼                           ▼
     ┌────────────────┐  ┌────────────────────┐   ┌────────────────────┐
     │ MarketState    │  │ StrategyManager    │   │ ExecutionHandler   │
     │ - OrderBook    │  │ - IStrategy        │   │ - Shadow book      │
     │ - InstrState   │  │   implementations  │   │ - Queue-position   │
     │ - Snapshots    │  │ - StrategyRegistry │   │   fill model       │
     └────────┬───────┘  └─────────┬──────────┘   └──────────┬─────────┘
              │                    │                         │
              └────────────────────┼─────────────────────────┘
                                   ▼
                       ┌──────────────────────────┐
                       │  PortfolioManager        │  positions, margin,
                       │                          │  realized/unrealized PnL
                       └──────────┬───────────────┘
                                  ▼
                       ┌──────────────────────────┐
                       │  ReportGenerator         │  CSV summary, equity curve,
                       │                          │  trade log
                       └──────────────────────────┘
```
 
The loop is currently single-threaded. Event ordering across data sources, strategies, and synthetic control events (snapshots, end-of-backtest) is total and deterministic, which makes replay reproducible bit-for-bit. A multi-threaded variant (producer-consumer with an SPSC ring between reader and event queue) is a roadmap item.
 
## Performance
 
All numbers are single-core, `-O3 -fno-omit-frame-pointer`, 16.1M MBO messages from a full ES futures session (`ES-glbx-20251105.mbo.csv.zst`, 41 active instrument IDs across normalized outrights and spreads).
 
### Data ingestion pipeline (MBO parse + dispatch)
 
| Stage                                                 | Throughput      | vs. baseline |
|-------------------------------------------------------|-----------------|--------------|
| Baseline (commit `3a8a473`)                           | 0.91 M msg/sec  | —            |
| Stream readers in `std::vector`, removed map lookup   | 2.21 M msg/sec  | **+143%**    |
 
### Order book hot path (`MarketStateManager::OnMarketEvent`)
 
| Stage                                                                 | Throughput     | vs. baseline |
|-----------------------------------------------------------------------|----------------|--------------|
| Baseline orderbook (commit `6c6738e`)                                 | 5.27 M evt/sec | —            |
| Sorted-vector levels worst→best, search from back                     | 11.19 M evt/sec| **+112%**    |
| `LIKELY`/`UNLIKELY` branch hints, vector-backed books                 | 11.30 M evt/sec| +115%        |
| Replaced `unordered_map` with custom probing table + backshift erase  | 15.90 M evt/sec| **+202%**    |
 
Each step was driven by `perf` + flame graphs. The full log including raw `perf stat` output (cycles, IPC, cache-miss rates, branch-miss rates) is in [BENCHMARKS.md](./BENCHMARKS.md).
 
The next planned optimization (called out in the benchmark log) is making cross-publisher BBO recomputation conditional on whether the publisher's TOB actually changed — currently ~24% of `OnMarketEvent` time.
 
## Validation
 
This project validates the OrderBook directly.
 
`OrderBook_test.cpp` runs the full-day ES session through `MarketStateManager` and, at every event with the F_LAST flag set, compares the reconstructed top-10 levels against the matching Databento MBP-10 snapshot (matched by `(ts_event, sequence, instrument_id, price)`). Every snapshot must match exactly: prices, sizes, and counts on both sides, to depth 10.
 
```cpp
EXPECT_EQ(actual_levels, expected_levels)
    << "Mismatch at ts_event = " << expected_mbp10_[instr_id].front().ts_event
    << " after " << events_processed << " MBO events";
```
 
This is replayed against ~16M events across 41 instruments, with no fault tolerance.

An independent oracle (scripts/oracle_barrier_test.py) cross-checks each simulated MovAvgCross entry against the raw trade tape, confirming the take-profit / stop-loss outcomes recorded by the simulator are supported by real prints.

Additional test coverage:
- `PortfolioManager_test.cpp` — open/close/flip, drawdown circuit breaker, margin-aware buying power, position-limit risk gate, invalid-tick rejection.
- `TimeUtils_test.cpp` — ISO-8601 parsing with nano-precision, fast 2/4-digit integer parsers, timezone offsets.
- `CsvZstReader_test.cpp` — streaming decompression edge cases (empty file, no trailing newline, lines larger than the decompressor's output buffer, re-open semantics).
- `ConfigParser_test.cpp` — JSON validation and type-safe required/optional field extraction.

## Quickstart

Tested on Ubuntu 22.04 / 24.04 with `g++` ≥ 11.

```bash
# Dependencies
sudo apt install cmake libzstd-dev g++ git

# Clone
git clone https://github.com/alegitdude/Backtester backtester && cd backtester

# Configure + build (Release)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j

# Fetch the demo dataset (342 MB ES futures MBO, hosted on GitHub Releases)
./scripts/fetch_demo_data.sh

# Run the demo backtest
./build/Backtester config/demo.json

# Run the tests
ctest --test-dir build --output-on-failure

# Run the benchmarks (always from a Release build)
./build/orderbook_perf_harness config/demo.json
./build/reader_perf_harness    config/demo.json
```

`nlohmann/json`, `spdlog`, and `googletest` are fetched automatically by CMake.

### Development builds

Optimization is driven entirely by `CMAKE_BUILD_TYPE` (Release → `-O3 -DNDEBUG`,
Debug → `-O0 -g`), so use a Debug tree for development and a Release tree for
anything you intend to benchmark or report from.

```bash
# Debug build with all warnings treated as errors
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DWARNINGS_AS_ERRORS=ON
cmake --build build-debug -j

# Debug build with AddressSanitizer + UBSan, for correctness runs
cmake -B build-san -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-san -j
ctest --test-dir build-san --output-on-failure
```

You only need to re-run `cmake -B <dir>` when you change `CMakeLists.txt` or
toggle an option; otherwise just re-run `cmake --build <dir>`. Benchmarks and
the demo report should always come from the **Release** tree —
`-O0` and sanitizer instrumentation change run speeds. 

### Demo dataset
The benchmark and demo configurations use a full session of
ES futures MBO data from Databento (16.1M messages, 342 MB compressed,
November 5, 2025). The file is hosted on the GitHub Releases page
and fetched by ./scripts/fetch_demo_data.sh to keep the repo lightweight.

## Configuration

Backtests are configured via a JSON file passed to the executable. The full
schema — every field, its type, units, defaults, and validation rules — is
documented in [`config/README.md`](./config/README.md).

A working example lives in [`config/demo.json`](./config/demo.json), which
also serves as the configuration for the sample reports found in `./docs/sample_movAvg_report`.

Brief overview of the top-level fields:

- `start_time` / `end_time` — ISO-8601 UTC bounds of the simulated session
- `traded_instruments` — instruments the strategy is allowed to trade,
  with tick size, tick value, and margin requirements
- `strategies` — which strategy classes to instantiate and their parameters
- `risk_limits` — drawdown circuit breaker, position-size caps, etc.
- `data_streams` — paths to MBO `.csv.zst` files and their schema/format flags

See [`config/README.md`](./config/README.md) for the full reference.
 
## Modeling Assumptions
 
Explicit list of things the simulator does and doesn't model, so results are interpreted honestly.
 
**Fills.** The default `QueuePosition` fill model tracks the depth ahead of a passive order at submission and fills only after sufficient size has executed at or through the order's price level. Marketable orders (crossing the spread) fill immediately at the opposite-side BBO. A `TopOfBook` model is available as an optimistic upper bound.
 
**Latency.** Orders submitted by a strategy are timestamped at submission but ineligible to fill until `submit_ts + execution_latency_ms` — a one-way model of wire time to the matching engine. No model of variable latency, packet loss, or queueing at the gateway.
 
**Unrealized PnL.** Marked at the global best bid (for shorts) or best ask (for longs), level 1 only. Large positions are not modeled as if they would walk the book on liquidation. Sufficient for strategies trading small relative size; would understate slippage for size that consumes multiple levels.
 
**Commissions.** Futures: flat per-contract (configurable). Equities: per-share with order minimum plus a clearing fee. No exchange-fee tiering, no make/take rebate modeling.
 
**Margin.** Initial and maintenance margin per futures contract; cash account for equities (no Reg-T or portfolio margin).
 
**Out of scope:** corporate actions, dividends, after-hours session boundaries, cross-product margin offsets, FX, and live trading.
 
## Scope & Limitations
 
This is a research backtester, not a live trading system. Specifically:
 
- **Single-threaded by design** — event ordering is total and the loop is deterministic. Throughput numbers are single-core.
- **MBO ingest only.** OHLCV ingest is scaffolded but not yet implemented; only CSV+zstd is supported (no DBN binary yet).
- **Single venue per instrument** at backtest time (the framework supports multiple publishers per instrument, but consolidated-book modeling assumes a single matching engine for fill simulation).
- **No transaction-cost analysis suite** beyond per-trade commission accounting.
## Project Layout
 
```
include/                  Public headers
  core/                   AppConfig, Event, EventQueue, ConfigParser, Backtester
  data_ingestion/         CsvZstReader, DataReaderManager
  market_state/           OrderBook, InstrumentState, MarketStateManager
  execution/              ExecutionHandler, FillModel
  portfolio/              PortfolioManager
  reporting/              ReportGenerator
  strategy/               IStrategy, StrategyManager, StrategyRegistry
  utils/                  TimeUtils, NumericUtils, StringUtils
 
src/                      Implementations, mirroring include/
src/strategy/user_strategies/   Drop new IStrategy subclasses here
 
test/                     GoogleTest suites mirroring src/
benchmarks/               Standalone perf harnesses for the reader and orderbook
config/                   Sample JSON configs
```

## Benchmarking & Profiling

To reproduce the **throughput numbers**, use the benchmark runner — no `perf`,
no root, no extra dependencies:

```bash
./scripts/fetch_demo_data.sh        # if you haven't already
./scripts/run_benchmarks.sh         # builds Release, runs both harnesses
```

It builds the harnesses in Release and runs each twice (override with
`RUNS=5 ./scripts/run_benchmarks.sh`), printing the `Throughput: … M/s` line
for the reader and the orderbook hot path. To run one directly instead:

```bash
./build/orderbook_perf_harness config/demo.json
./build/reader_perf_harness    config/demo.json
```

To reproduce the **flame graphs** in [BENCHMARKS.md](./docs/BENCHMARKS.md), you
need `perf` and Brendan Gregg's FlameGraph scripts:

```bash
# One-time setup
sudo apt install linux-tools-common linux-tools-generic linux-tools-$(uname -r)
git clone https://github.com/brendangregg/FlameGraph   # into the repo root

# Build the harnesses in Release. Frame pointers (-fno-omit-frame-pointer) are
# on in every build type, so the optimized binary still has named stack frames.
./scripts/build_bench.sh

# Record a profile, then render the flame graph. Both scripts take the harness
# name and write to benchmarks// so the two profiles don't collide.
./scripts/profile.sh        orderbook_perf_harness
./scripts/generate_flame.sh orderbook_perf_harness   # -> benchmarks/orderbook_perf_harness/orderbook_perf_harness_flame.svg

# Same for the ingestion reader:
./scripts/profile.sh        reader_perf_harness
./scripts/generate_flame.sh reader_perf_harness
```

`profile.sh` uses `sudo perf record`. If you'd rather not profile as root,
lower the paranoia level once with `sudo sysctl kernel.perf_event_paranoid=1`
and drop the `sudo` from the script.

## Analysis & Visualization

After a backtest, the `scripts/create_equity_curve_graph.py` script renders the equity
curve from `reports/equity_curve.csv` into the reports folder:

```bash
# From the project root
python3 -m venv .venv
source .venv/bin/activate
pip install -r scripts/requirements.txt

python3 scripts/create_equity_curve_graph.py
```

A virtual environment isn't strictly required, but is recommended to avoid
polluting your system Python. The same script run without a venv just needs
`pip install -r scripts/requirements.txt --user`.

A sample chart from the demo run is committed at
[`docs/sample_movAvg_report/equity_curve_plot.png`](docs/sample_movAvg_report/equity_curve_plot.png).

## License
 
MIT — see `LICENSE`.
