# Config Reference
 
The backtester reads a JSON config file describing what to run: time range,
which instruments, which strategies, what risk limits, and where to find data.
This document describes every field the parser recognizes.
 
A complete working example is in [`demo.json`](./demo.json).

---
## Table of Contents
- [Conventions](#conventions)
- [Top-Level Fields](#top-level-fields)
- [Traded_Instruments](#traded-instruments)
- [Strategies](#strategies)
- [Risk Limits](#risk-limits)
- [Data Streams](#data-streams)
- [Commissions](#commissions)
---
 
## Conventions
 
**Fixed-point prices and dollars.** All monetary values inside the running
system (prices, cash, PnL, margin), are stored as 64-bit integers scaled by
1,000,000,000 (1e9). Where the JSON config accepts a "decimal" value, the
parser converts it on load. For example, a `tick_size` of `0.25` in JSON becomes
`250000000` internally. 
 
This avoids floating-point error in price arithmetic. You write decimal numbers
in JSON; the system reasons in integers.
 
**Timestamps.** All internal timestamps are unsigned 64-bit integers counting
nanoseconds since the Unix epoch. The config accepts ISO-8601 strings, which
the parser converts on load.

**Path resolution:** Relative file paths in the config are resolved against
the directory containing the config file, not the current working directory.
So if `demo.json` lives at `config/demo.json` and references
`"../test/test_data/foo.csv.zst"`, the file is looked up at
`test/test_data/foo.csv.zst` regardless of where the backtester is invoked
from. Absolute paths are used as-is.
 
**Units summary:**
 
| Where it appears        | JSON form        | Internal form                   |
|-------------------------|------------------|---------------------------------|
| Prices, tick size/value | decimal number   | int64, scaled by 1e9            |
| Cash, margin, PnL       | integer          | int64, scaled by 1e9            |
| Risk percentages        | decimal fraction | int64, scaled by 1e9 (e.g. 2% → 20_000_000) |
| Timestamps              | ISO-8601 string  | uint64 nanoseconds since epoch  |
| Latency, intervals      | integer ms or ns | uint64 ms or ns                 |
 

---
 
## Top-Level Fields
 
### `start_time` *(required, string)*
 
ISO-8601 UTC timestamp marking when strategies begin trading. Market events
before this time are still processed so the order book and any rolling state
warm up correctly, but strategies do not receive signals and orders are not
issued.

Accepts YYYY-MM-DDTHH:MM:SS with an optional . and 1–9 fractional-second digits. 
The timestamp is always interpreted as UTC; a trailing Z is accepted but optional, 
and any timezone offset is not supported.
 
Example: `"2025-11-05T14:30:00.000000000Z"`.
 
### `end_time` *(required, string)*
 
ISO-8601 UTC timestamp marking the end of the backtest. After this point, any
open positions are flattened by emitting closing orders at the prevailing BBO,
and reports are written.
 
Same format as `start_time`. Must be strictly greater than `start_time`.

### `initial_cash` *(optional, integer)*
 
Account starting balance in dollars (not fixed-point — the parser scales it).
Default: `100000`.
 
Must be positive. A value below 1 falls back to the default with a warning.
 
### `execution_latency_ms` *(optional, integer)*
 
Latency in milliseconds between order submission and order eligibility
to fill in the execution simulator. Designed to simulate real trading latencies.
Default: `200`.
 
A strategy that emits an order at timestamp T will not be considered for
matching until T + (latency_ms × 1,000,000) nanoseconds.
 
### `snapshot_interval_ms` *(optional, integer)*
 
How often the report generator records an equity snapshot during the run, in
milliseconds. Default: `1000` (one second).
 
Smaller intervals produce a higher-resolution equity curve at the cost of
larger output files.
 
### `log_file_path` *(optional, string)*
 
Directory where spdlog writes the simulation log. Default: `../logs`. The
directory must exist; the parser does not create it.
 
### `report_output_dir` *(optional, string)*
 
Directory where the report generator writes `summary.csv`, `trade_log.csv`,
`equity_curve.csv`, and (if multiple strategies are running)
`strategy_breakdown.csv`. Default: `../reports`. Created if it does not exist.

### `risk_free_rate` *(optional, decimal)*
Represents the current risk-free rate used when calculating Sharpe and Sortino ratios of a finished strategy backtest. Default: `.05` (5%)

---
## Traded Instruments
### `traded_instruments` *(required, array of objects)*
 
Each entry details an instrument a strategy is permitted to trade. The
backtester enforces this list — strategies attempting to submit orders for
instruments not in this array will be rejected with an error.
 
At least one entry is required.
 
### Per-entry fields
 
#### `instrument_id` *(required, integer)*
 
The numeric instrument ID assigned by the data vendor (e.g. Databento). Used
to identify the instrument throughout the system.
 
#### `instrument_type` *(required, string)*
 
One of `"FUT"`, `"STOCK"`, `"OPTION"`. Determines PnL accounting and margin
treatment:
 
- `FUT`: PnL computed as `ticks_captured × tick_value × contracts`. Margin
  reserved per contract from `init_margin_req`.
- `STOCK`: PnL computed as `(exit_price - entry_price) × shares`. "Margin" is
  the full notional (cash account model).
- `OPTION`: declared but not currently implemented.
#### `tick_size` *(required, decimal)*
 
The minimum price increment for the instrument, as a decimal. For ES futures
this is `0.25`. The parser scales this to fixed point on load.
 
Orders submitted at prices that are not exact multiples of `tick_size` are
rejected by the portfolio manager.
 
Must be greater than zero.
 
#### `tick_value` *(required, decimal)*
 
Dollar value of one tick for one contract/share. For ES futures this is
`12.50` (each 0.25 point on one ES contract is $12.50). The parser scales this
to fixed point on load.
 
Used to convert price moves into PnL for futures.
 
Must be greater than zero.
 
#### `init_margin_req` *(required, decimal)*
 
Initial margin requirement to open one contract, in dollars. Reserved from
buying power at order submission. Released on open, close, or cancel.
 
#### `main_margin_req` *(required, decimal)*
 
Maintenance margin requirement to hold one contract, in dollars. Held against
buying power for the life of an open position.
 
Typically `main_margin_req ≤ init_margin_req` for futures contracts.
 
---
## Strategies 
### `strategies` *(required, array of objects)*
 
List of strategies to instantiate at startup. Each strategy must be registered
via the `REGISTER_STRATEGY` macro in its `.cpp` file.
 
At least one entry is required.
 
### Per-entry fields
 
#### `name` *(required, string)*
 
The strategy's registration name (the second argument to `REGISTER_STRATEGY`
in the strategy's source file). Must match exactly. If no strategy with this
name is registered, startup fails with an error.
 
Example: `"MovAvgCrossMin"`.
 
#### `params` *(required, array of integers)*
 
Strategy-specific parameters. Interpretation depends on the strategy:
 
- `MovAvgCrossMin`: `[fast_window, slow_window]`. Both must be positive and
  `fast_window < slow_window`.
Refer to each strategy's `.cpp` file for its parameter contract.
 
#### `max_lob_lvl` *(required, integer)*
 
Maximum order book depth the strategy needs to read. This can allow the backtester
further speed optimizations if only fractions of the orderbook need to be utilized for strategy calculations. 
 
#### `traded_instr_id` *(required, integer)*
 
The numeric instrument ID this strategy trades. Must appear in the
`traded_instruments` array. Currently strategies are single-instrument trading, 
though can generate signal from all instruments in the data provided. This
field tells the strategy which one is theirs.
 
---
## Risk Limits 
### `risk_limits` *(optional, object)*
 
Portfolio-level risk gates. Checked on every order submission.
 
### `risk_mode` *(string)*
 
One of `"PercentOfAcct"` (the default) or `"PosSizeInDollars"`. Determines how
`max_position_size` is interpreted.
 
- `PercentOfAcct`: position-size limits expressed as integer contract counts
- `PosSizeInDollars`: position-size limits expressed in fixed-point dollars
Currently the parser accepts both but the portfolio manager treats
`max_position_size` uniformly as contract count. Effectively `PercentOfAcct`
is the only fully implemented mode.
 
### `max_position_size` *(decimal)*
 
Maximum absolute position size per instrument, in contracts. A value of `0`
disables the check. Default: `3`.
 
Orders that would result in `|position| > max_position_size` are rejected.
 
### `max_risk_per_trade_pct` *(decimal fraction)*
 
Maximum fraction of account equity the strategy is permitted to risk on a
single trade. Default: `0.02` (2%).
 
Note: this field is parsed and stored but **not currently enforced by the
portfolio manager**. Listed here for completeness.
 
### `max_portfolio_delta` *(decimal)*
 
Maximum aggregate dollar delta (signed) across all open positions. A value of
`0` disables the check. Default: `0`.
 
Not currently enforced.
 
### `max_drawdown_pct` *(decimal fraction)*
 
Drawdown circuit breaker as a fraction of peak equity. Default: `0.40` (40%).
When current drawdown exceeds this value, all new orders are rejected.
 
Computed as `(peak_equity - current_equity) / peak_equity`.
 
### `max_delta_per_trade` *(decimal)*
 
Maximum dollar delta a single trade may add to the portfolio. A value of `0`
disables the check. Default: `0`.
 
Not currently enforced.
 
---
## Data Streams
### `data_streams` *(required, array of objects)*
 
Data sources to ingest. Each describes one MBO/OHLCV file the reader will
process. Multiple streams are supported (e.g. concurrent ES + NQ feeds).
 
At least one entry is required.
 
### Per-entry fields
 
#### `data_source_name` *(required, string)*
 
A human-readable name for the stream. Used to dispatch events back to the
correct reader when loading the next event from a given source. Must be
unique across streams in the same config.
 
#### `symbology_filepath` *(required, string)*
 
Path to the symbology CSV that maps trading symbols to instrument IDs for
this stream. The file must have a header row followed by rows of
`symbol,instrument_id,date`.
 
#### `data_filepath` *(required, string)*
 
Path to the data file. Currently must be a zstd-compressed CSV in Databento
MBO schema (header row `ts_recv,ts_event,rtype,publisher_id,instrument_id,
action,side,price,size,channel_id,order_id,flags,ts_in_delta,sequence,symbol`).
 
#### `schema` *(required, string)*
 
One of `"MBO"`, `"OHLCV"`. Currently only `MBO` is fully implemented.
 
#### `encoding` *(required, string)*
 
One of `"CSV"`, `"DBN"`, `"JSON"`. Currently only `CSV` is implemented.
 
#### `compression` *(required, string)*
 
One of `"ZSTD"`, `"NONE"`. Currently only `ZSTD` is implemented for streaming
ingest.
 
#### `price_format` *(required, string)*
 
One of `"decimal"` or `"fixpntint"`. Tells the parser how prices are stored
in the data file:
 
- `decimal`: e.g. `4500.25`. The parser multiplies by 1e9 on read.
- `fixpntint`: e.g. `4500250000000`. The parser reads directly.
Choose the one matching your data export. Databento decimal CSV exports use
`decimal`; raw DBN-equivalent exports use `fixpntint`.
 
#### `timestamp_format` *(required, string)*
 
One of `"iso"` or `"unix"`. Tells the parser how timestamps are stored:
 
- `iso`: ISO-8601 string, e.g. `2025-11-05T14:30:00.000000000Z`. Slower to parse.
- `unix`: integer nanoseconds since epoch. Faster.
For high-throughput backtests on large files, prefer `unix` if your data
exporter supports it.
 
---
## Commissions
### `commissions` *(optional, object)*
 
Commission structure for futures and stocks. Defaults based on current average Interactive Broker Fixed Pricing rates. 
 
### `fut_per_contract` *(optional, decimal)*
 
Flat rate per traded futures contract. Default: `2.17` ($2.17)
 
### `stock_order_min` *(optional, decimal)*
 
The minimum fee for a stock transaction. If a stock transaction on a per-share basis does not meet the minimum threshold, the transaction is charged the minimum fee. Default `.35`
 
### `stock_per_share` *(optional, decimal)*
 
Commission charged per share of a stock transaction. Commission only kicks in if total commission is above the stock order minimum. Default: `.035` 
 
### `stock_clearing_fee` *(optional, decimal)*
A fee charged by the NSCC and DTC for clearing stock purchases on a per-share basis. Often excluded from some broker's fees when using a tiered system, but added in this project. Default: `0002`

 
---