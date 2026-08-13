# Event-Driven Backtester for High-Frequency Market Data

[![CI](https://github.com/alegitdude/Backtester/actions/workflows/ci.yml/badge.svg)](https://github.com/alegitdude/Backtester/actions/workflows/ci.yml)

A multithreaded, event-driven backtesting framework in C++20 for futures and equities strategies, built around Databento MBO (Market-By-Order) feeds. Market data is parsed on a producer thread and handed to a consumer over a lock-free SPSC ring, while replay stays bit-for-bit deterministic — the threaded output is verified byte-identical to a single-threaded reference. Reconstructs the full limit order book from order-level data, replays strategies against historical events, and produces per-trade and equity-curve reports.
 
Reconstructs the full venue limit order book from MBO data at 15.9 M events/sec single-core, driven by a custom probing hash table and cache-friendly level structure (a +202% gain over the baseline, each step measured with perf + flame graphs). The multithreaded pipeline runs producer/consumer over a lock-free SPSC ring and stays bit-for-bit deterministic. Output verified byte-identical to the single-threaded reference and race-free under ThreadSanitizer.
 
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

- **Custom open-addressing hash table with backshift deletion** for order-ID lookup, replacing `std::unordered_map`. Drove the orderbook hot path from 11.3 → 15.9 M events/sec (ThinkPad T1) by collapsing cache-miss-heavy chained-bucket scans into linear probes over contiguous memory.
- **Lock-free SPSC ring on the market-data path.** A cache-line-padded, power-of-two, acquire/release single-producer/single-consumer ring hands fixed-size events from the parser thread to the book-building thread with a zero-copy peek/commit API. The producer k-way merges N sources into one timestamp-ordered stream; the consumer two-way merges that against consumer-local synthetic events. Verified race-free under ThreadSanitizer over full-day replays and byte-identical to the single-threaded loop.
- **Order book reconstruction from MBO data.** Per-publisher books aggregated into a consolidated instrument-level BBO. Handles add / modify / cancel / clear / trade actions with F_LAST flag-driven BBO cache updates.
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
   (N sources)         │  (per-source readers)    │
                       └──────────┬───────────────┘
                                  │  MarketByOrderEvent
                                  ▼
        ┌─────────────────────────────────────────────┐
        │  PRODUCER THREAD                             │
        │  k-way merge across sources (min-heap on ts) │  globally ts-ordered
        └──────────────────────┬──────────────────────┘
                               │  writes in place, no copy
                               ▼
                 ╔═════════════════════════════╗
                 ║  Lock-free SPSC ring        ║   single-producer /
                 ║  (cache-line-padded cursors)║   single-consumer handoff
                 ╚══════════════╤══════════════╝
                                │  acquire/release, zero-copy peek
                                ▼
        ┌─────────────────────────────────────────────┐
        │  CONSUMER THREAD                            │
        │  two-way merge: ring (market) vs.           │
        │  EventQueue (synthetic), earliest ts wins   │
        └──────────────────────┬──────────────────────┘
                               │
              ┌────────────────┼───────────────────────────┐
              ▼                ▼                            ▼
     ┌────────────────┐  ┌────────────────────┐   ┌────────────────────┐
     │ MarketState    │  │ StrategyManager    │   │ ExecutionHandler   │
     │ - OrderBook    │  │ - IStrategy        │   │ - Shadow book      │
     │ - InstrState   │  │   implementations  │   │ - Queue-position   │
     │ - Snapshots    │  │ - StrategyRegistry │   │   fill model       │
     └────────┬───────┘  └─────────┬──────────┘   └──────────┬─────────┘
              │                    │                         │
              │       synthetic events (signals, orders,     │
              │       fills, control) ─► EventQueue ─────────┘
              │       (min-heap by (ts,type), consumer-local)
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
Market data flows producer → consumer across a lock-free single-producer / single-consumer (SPSC) ring. The producer thread parses every source and k-way merges them into one globally timestamp-ordered stream; the consumer thread two-way merges that stream against the EventQueue of synthetic events (strategy signals, orders, fills, end-of-backtest), which are generated consumer-side and never cross the thread boundary.

That split is what keeps replay bit-for-bit deterministic despite being concurrent: market events arrive in FIFO order over the ring, synthetic events are produced in a fixed order by the single consumer, and the merge is a pure function of timestamps. The threaded run's trade log, equity curve, and summary are verified byte-identical to the single-threaded reference loop, and the pipeline is verified race-free under ThreadSanitizer over full-day replays. A single-threaded loop is retained as both the determinism oracle and the performance baseline (select with the single / threaded run argument).
 
## Performance
### End-to-end pipeline (full backtest, Lenovo Flex 5, 4 cores) ###

| Mode	                             | Wall clock |	Throughput	   | Speedup  |
|------------------------------------|------------|----------------|----------|
| Single-threaded reference (1 core) | 17.42 s	  | 0.90 M evt/sec |	—       |
| Threaded producer/consumer (2 core)| 14.26 s	  | 1.10 M evt/sec |	1.22×   |

Median of 10 runs each, pinned to distinct physical cores. The speedup is producer-bound by design: parsing dominates the per-event budget (~1.06 µs vs. ~0.14 µs for book apply), so the consumer spends much of its time waiting on the ring. See BENCHMARKS.md for the full analysis.

### Data ingestion pipeline (MBO parse + dispatch) (ThinkPad T1)
 
| Stage                                                 | Throughput      | vs. baseline |
|-------------------------------------------------------|-----------------|--------------|
| Baseline (commit `3a8a473`)                           | 0.91 M msg/sec  | —            | 
| Stream readers in `std::vector`, removed map lookup   | 2.21 M msg/sec  | **+143%**    | 
 
### Order book hot path (`MarketStateManager::OnMarketEvent`) (ThinkPad T1)
 
| Stage                                                                 | Throughput     | vs. baseline |
|-----------------------------------------------------------------------|----------------|--------------|
| Baseline orderbook (commit `6c6738e`)                                 | 5.27 M evt/sec | —            | 
| Sorted-vector levels worst→best, search from back                     | 11.19 M evt/sec| **+112%**    |
| `LIKELY`/`UNLIKELY` branch hints, vector-backed books                 | 11.30 M evt/sec| +115%        |
| Replaced `unordered_map` with custom probing table + backshift erase  | 15.90 M evt/sec| **+202%**    |
 
Each step was driven by `perf` + flame graphs. The full log including raw `perf stat` output (cycles, IPC, cache-miss rates, branch-miss rates) is in [BENCHMARKS.md](./docs/BENCHMARKS.md).
 
Roadmap: per-event latency histograms (p50/p99/p99.9) to characterize tail latency alongside throughput; explicit thread pinning of the producer/consumer to isolated cores.
 
## Validation
 
This project validates the OrderBook directly.
 
`OrderBook_test.cpp` runs the full-day ES session through `MarketStateManager` and, at every event with the F_LAST flag set, compares the reconstructed top-10 levels against the matching Databento MBP-10 snapshot (matched by `(ts_event, sequence, instrument_id, price)`). Every snapshot must match exactly: prices, sizes, and counts on both sides, to depth 10.
 
```cpp
EXPECT_EQ(actual_levels, expected_levels)
    << "Mismatch at ts_event = " << expected_mbp10_[instr_id].front().ts_event
    << " after " << events_processed << " MBO events";
```
 
This is replayed against ~16M events across 41 instruments, with no fault tolerance.

**Note:** if you do not download the full-day ES data, the test will run using a thin sample of 500k messages from ./test/test_data against the sample mbp-10 file.   

An independent oracle (scripts/oracle_barrier_test.py) cross-checks each simulated MovAvgCross entry against the raw trade tape, confirming the take-profit / stop-loss outcomes recorded by the simulator are supported by real prints.

#### Deterministic concurrency #### 
The threaded pipeline's trade log, equity curve, and summary are verified byte-identical to the single-threaded reference loop on the full ES session, and the two-thread run is verified race-free under ThreadSanitizer across full-day replays. Determinism holds because market events cross the SPSC ring in FIFO order while synthetic events stay consumer-local, so the merge is a pure function of timestamps regardless of thread scheduling.

#### Additional test coverage: ####
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

# Run the demo, two options:
# (a) No download: runs on the committed 500k-message sample (overnight
#     globex ES). Proves the pipeline end-to-end — trade log, equity curve,
#     full metrics — but it's a sample test, not a meaningful strategy result.
./build/Backtester config/demo_sample.json

# (b) Full session: fetch the 342 MB dataset first, then run demo.json.
#     This is the config behind the headline performance and validation numbers.
./scripts/fetch_demo_data.sh
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

# Debug build with TSAN
sudo sysctl vm.mmap_rnd_bits=28   # TSan/ASLR workaround, resets on reboot
cmake -S . -B build-tsan -DENABLE_TSAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build-tsan --target Backtester
./build-tsan/Backtester config/demo.json threaded
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

A 500k message sample dataset is also included in test/test_data for verification
the backtester will actually run, but is not enough data to meaninfully test a strategy
or confirm much of the backtester behavior. 

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
 
- **Multithreaded, deterministically.** Market-data parsing and book/strategy processing run on separate threads across a lock-free SPSC ring; a single-threaded reference loop is retained as the determinism oracle and baseline. Event ordering is total and replay is bit-for-bit reproducible in both modes. The end-to-end speedup is producer-bound (see Performance).
- **MBO ingest only.** OHLCV ingest is scaffolded but not yet implemented; only CSV+zstd is supported (no DBN binary yet).
- **Single venue per instrument** at backtest time (the framework supports multiple publishers per instrument, but consolidated-book modeling assumes a single matching engine for fill simulation).
- **No transaction-cost analysis suite** beyond per-trade commission accounting.
## Project Layout
 
```
include/                  Public headers
  core/                   AppConfig, Event, EventQueue, SPSC Ring, ConfigParser, Backtester
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
./scripts/harness_profile.sh   orderbook_perf_harness
./scripts/generate_flame.sh    orderbook_perf_harness   # -> benchmarks/orderbook_perf_harness/orderbook_perf_harness_flame.svg

# Same for the ingestion reader:
./scripts/harness_profile.sh    reader_perf_harness
./scripts/generate_flame.sh     reader_perf_harness
```

`harness_profile.sh` uses `sudo perf record`. If you'd rather not profile as root,
lower the paranoia level once with `sudo sysctl kernel.perf_event_paranoid=1`
and drop the `sudo` from the script.

## Analysis & Visualization

After a backtest, the `scripts/create_equity_curve_graph.py` script renders the equity
curve from `reports/equity_curve.csv` into the reports folder. This script requires 
Python 3. On Ubuntu/Debian systems, you must explicitly install the Python 
virtual environment package and pip:

```bash
sudo apt update && sudo apt install -y python3-venv python3-pip
```
Then you can run the script to create the equity curve graph:

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
 
MIT — see ['LICENSE'](LICENSE).
