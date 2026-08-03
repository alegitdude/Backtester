# Performance Optimization Log: Rebase (Lenovo Flex 5)

## Hardware & Environment
* **CPU:** Intel(R) Core(TM) i7-8550U CPU @ 1.80GHz
* **Cores:** 8 available
* **Ram:** 15 Gbs
* **OS:** Linux (Ubuntu)
* **Compiler:** GCC (g++) with -O3 -fno-omit-frame-pointer
* **Data Source:** Databento MBO (16.158945 million messages, zstd compressed)

* **Methodology:** Benchmarks run with all cores set to performance, pinned to a single physical core via taskset. Fixed clocks eliminate thermal-throttling drift on this laptop; absolute times are therefore lower than peak-boost runs, but run-to-run variance is lower. Page cache warmed.

---
## Switch To POD event structure Full Backtest Throughput (Lenovo FLex 5)
**Date:** August 2, 2026
**Commit:** `0c50161`
```text
== Full backtest benchmark ==
  binary=/home/r/Desktop/Backtester/build/Backtester
  config=/home/r/Desktop/Backtester/config/demo.json
  core=2  runs=10

  run  1/10:  17.608s   0.89 M evt/s   RSS 347 MB
  run  2/10:  17.703s   0.88 M evt/s   RSS 347 MB
  run  3/10:  17.539s   0.89 M evt/s   RSS 346 MB
  run  4/10:  17.544s   0.89 M evt/s   RSS 346 MB
  run  5/10:  17.453s   0.89 M evt/s   RSS 347 MB
  run  6/10:  17.550s   0.89 M evt/s   RSS 347 MB
  run  7/10:  17.380s   0.90 M evt/s   RSS 346 MB
  run  8/10:  17.519s   0.89 M evt/s   RSS 347 MB
  run  9/10:  17.338s   0.90 M evt/s   RSS 347 MB
  run 10/10:  17.700s   0.88 M evt/s   RSS 347 MB

== Median over 10 runs ==
  RunLoop:     17.541 s   (min 17.338 / max 17.703)
  Throughput:  0.890 M evt/s
  Peak RSS:    347 MB

```

## Restructure Events to be Pieces of Data, remove all strings from hot path (Lenovo FLex 5)
**Date:** August 2, 2026
**Commit:** `0c50161`

### Results
* **Throughput:** 1.23718 - 1.22205 Million messages / second
* **Time to Process 16M messages:** 13.0611s - 13.2228s 
* **Bottlenecks Identified:**  ReadLine has a lot of cpu cycles that aren't decompression, 
perhaps getting rid of GetNextToken and reading a whole line in a loop decreases the function calls enough to make a difference.
### Raw Perf Output
```text
== Environment ==
  harness=reader_perf_harness  core=2  config=/Backtester/config/demo.json

== perf record -> /Backtester/benchmarks/reader_perf_harness/perf.data ==
Processed 16158945 messages.
Time: 13.1097s (1.23259 M/s)
total_volume: 28907885
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.165 MB /Backtester/benchmarks/reader_perf_harness/perf.data (1299 samples) ]

== perf stat (group A: time, cycles, instructions) ==
Processed 16158945 messages.
Time: 13.0611s (1.23718 M/s)
total_volume: 28907885

== perf stat (group B: branches, cache hierarchy) ==
Processed 16158945 messages.
Time: 13.2228s (1.22205 M/s)
total_volume: 28907885

perf stat summary  harness=reader_perf_harness  core=2  2026-08-02T17:54:12-05:00
config=/Backtester/config/demo.json

       23452589587  cycles
       67936424988  instructions
       15673302321  branches
         112734842  branch-misses
       12202432003  L1-dcache-loads
         161917116  L1-dcache-load-misses
           7653566  LLC-loads
           2153542  LLC-load-misses

      13.065104763  seconds time elapsed
       0.000013062  seconds task-clock (on-CPU)

---- ratios ----
IPC:          2.90
branch-miss:  0.72%

---- cache hierarchy ----
L1-dcache miss:  1.33%   (161917116 / 12202432003 loads)
LLC-load miss:   28.14%   (2153542 / 7653566 LLC refs)
LLC refs are 0.063% of L1 loads; ~0.018% of all loads reach DRAM.
(LLC ref count is tiny, so a high LLC-miss % is expected and healthy.)

```

## [Baseline] Initial Full Backtest Throughput (Lenovo FLex 5)
**Date:** July 25, 2026
**Commit:** `f390e7c`
```text
== Full backtest benchmark ==
  binary=/home/r/Desktop/Backtester/build/Backtester
  config=/home/r/Desktop/Backtester/config/demo.json
  core=2  runs=10

  run  1/10:  20.158s   0.77 M evt/s   RSS 346 MB
  run  2/10:  22.480s   0.69 M evt/s   RSS 346 MB
  run  3/10:  20.686s   0.75 M evt/s   RSS 346 MB
  run  4/10:  20.665s   0.75 M evt/s   RSS 346 MB
  run  5/10:  20.514s   0.76 M evt/s   RSS 346 MB
  run  6/10:  21.201s   0.74 M evt/s   RSS 346 MB
  run  7/10:  20.349s   0.77 M evt/s   RSS 346 MB
  run  8/10:  20.706s   0.75 M evt/s   RSS 346 MB
  run  9/10:  20.558s   0.76 M evt/s   RSS 346 MB
  run 10/10:  20.569s   0.76 M evt/s   RSS 346 MB

== Median over 10 runs ==
  RunLoop:     20.617 s   (min 20.158 / max 22.480)
  Throughput:  0.755 M evt/s
  Peak RSS:    346 MB

```

## [Baseline] Initial Orderbook Throughput (Lenovo FLex 5)
**Date:** July 25, 2026
**Commit:** `f390e7c`

### Results
* **Throughput:** 6.86402 - 7.16727 Million messages / second
* **Time to Process 16M messages:** 2.35415s - 2.25455s
* **Bottlenecks Identified:**  

### Raw Perf Output
```text
== Environment ==
  harness=orderbook_perf_harness  core=2  config=/Backtester/config/demo.json

== perf record -> /Backtester/benchmarks/orderbook_perf_harness/perf.data ==
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 2.35415s
Throughput: 6.86402 M/s
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.268 MB /Backtester/benchmarks/orderbook_perf_harness/perf.data (2203 samples) ]

== perf stat (group A: time, cycles, instructions) ==
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 2.25213s
Throughput: 7.17495 M/s

== perf stat (group B: branches, cache hierarchy) ==
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 2.25455s
Throughput: 7.16727 M/s

perf stat summary  harness=orderbook_perf_harness  core=2  2026-07-25T11:00:05-05:00
config=/Backtester/config/demo.json

       39040390107  cycles
       90756433480  instructions
       21220897890  branches
         164056372  branch-misses
       18108356810  L1-dcache-loads
         324714166  L1-dcache-load-misses
          46843627  LLC-loads
          19071805  LLC-load-misses

      21.872506032  seconds time elapsed
       0.000021753  seconds task-clock (on-CPU)

---- ratios ----
IPC:          2.32
branch-miss:  0.77%

---- cache hierarchy ----
L1-dcache miss:  1.79%   (324714166 / 18108356810 loads)
LLC-load miss:   40.71%   (19071805 / 46843627 LLC refs)
LLC refs are 0.259% of L1 loads; ~0.105% of all loads reach DRAM.
(LLC ref count is tiny, so a high LLC-miss % is expected and healthy.)

```

## [Baseline] Initial Data Ingestion Implementation (Lenovo FLex 5)
**Date:** July 25, 2026
**Commit:** `f390e7c`

### Results
* **Throughput:** .939169 - .999125 Million messages / second
* **Time to Process 16M messages:** 16.1731s - 17.2056s
* **Bottlenecks Identified:**  
### Raw Perf Output
```text
== Environment ==
  harness=reader_perf_harness  core=2  config=/Backtester/config/demo.json

== perf record -> /Backtester/benchmarks/reader_perf_harness/perf.data ==
Processed 16158945 messages.
Time: 17.2056s (0.939169 M/s)
total_volume: 28907885
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.205 MB /Backtester/benchmarks/reader_perf_harness/perf.data (1702 samples) ]

== perf stat (group A: cycles, instructions) ==
Processed 16158945 messages.
Time: 16.1731s (0.999125 M/s)
total_volume: 28907885

== perf stat (group B: branches, cache) ==
Processed 16158945 messages.
Time: 16.4677s (0.981252 M/s)
total_volume: 28907885

perf stat summary  harness=reader_perf_harness  core=2  2026-07-25T10:29:49-05:00
config=/Backtester/config/demo.json

       28888762779  cycles
       77940173988  instructions
       18595926495  branches
         125844425  branch-misses
         106732049  cache-references
          11857361  cache-misses

      16.178810517  seconds time elapsed
       0.000016101  seconds task-clock (on-CPU)

---- ratios ----
IPC:          2.70
branch-miss:  0.68%
cache-miss:   11.11%

```

# Performance Optimization Log: Orderbook Operations (Thinkpad T1)

## Replaced unordered_map with probing table w/backshift deletion for order maintenance 
**Date:** May 5, 2026
**Commit:** `81fc075`

### Results
* **Throughput:** 15.7863 - 15.9005 Million messages / second
* **Time to Process 16M messages:** 1.0236s - 1.01625s
* **Bottlenecks Identified:**  Need to figure out how to get updating the instrument bbo_cache to be less expensive/less frequent, possibly return a bool that TOB is updated? Taking up ~24% of total OnMarketEvent method. Also cache-misses are still too high.

### Raw Perf Output
```text
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.01625s
Throughput: 15.9005 M/s
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.172 MB /home/r/Desktop/Backtester/benchmarks/orderbook_perf_harness/perf.data (1284 samples) ]

Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.0236s
Throughput: 15.7863 M/s

 Performance counter stats for './orderbook_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

    13,732,410,914      task-clock                       #    1.000 CPUs utilized             
    41,266,541,145      cycles                           #    3.005 GHz                         (83.33%)
    94,724,420,318      instructions                     #    2.30  insn per cycle              (83.32%)
       957,845,306      cache-references                 #   69.751 M/sec                       (83.34%)
        58,843,945      cache-misses                     #    6.14% of all cache refs           (83.33%)
    21,865,570,639      branches                         #    1.592 G/sec                       (83.34%)
       244,297,049      branch-misses                    #    1.12% of all branches             (83.33%)

      13.737871362 seconds time elapsed

      11.687111000 seconds user
       2.046319000 seconds sys
```

***

## Implemented LIKELY/UNLIKELY in branches and books are now stored in vectors
**Date:** April 24, 2026
**Commit:** `b11be3b`

### Results
* **Throughput:** 11.3051 - 11.5048 Million messages / second
* **Time to Process 16M messages:** 1.42934s - 1.40454s
* **Bottlenecks Identified:** orders_by_id still taking up large amounts of cpu cycles, hashtable operations are large percentage of apply operations especially in the Cancel method. This could possibly explain the large percentage of cache misses, which really needs addressing. Also need to figure out how to get updating the instrument bbo_cache to be less expensive/less frequent

### Raw Perf Output
```text
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.42934s
Throughput: 11.3051 M/s
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.173 MB /home/r/Desktop/Backtester/benchmarks/orderbook_perf_harness/perf.data (1279 samples) ]

Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.40454s
Throughput: 11.5048 M/s

 Performance counter stats for './orderbook_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

    14,411,253,978      task-clock                       #    1.000 CPUs utilized             
    43,425,872,366      cycles                           #    3.013 GHz                         (83.32%)
    96,600,023,684      instructions                     #    2.22  insn per cycle              (83.34%)
     1,023,563,501      cache-references                 #   71.025 M/sec                       (83.33%)
        77,749,717      cache-misses                     #    7.60% of all cache refs           (83.34%)
    22,181,845,174      branches                         #    1.539 G/sec                       (83.33%)
       237,529,840      branch-misses                    #    1.07% of all branches             (83.34%)

      14.416618411 seconds time elapsed

      12.342509000 seconds user
       2.070582000 seconds sys
```

## OB Levels are sorted vectors worst->best with level searching from end of vector
**Date:** April 19, 2026
**Commit:** `beb262c`

### Results
* **Throughput:** 11.1948 Million messages / second
* **Time to Process 16M messages:** 1.46501s
* **Bottlenecks Identified:** Updating Instrument BBO taking too much cpu power, need to find a way to optimize the loop or different caching strategy

### Raw Perf Output
```text
Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.44343s
Throughput: 11.1948 M/s
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.149 MB /home/r/Desktop/Backtester/benchmarks/orderbook_perf_harness/perf.data (1072 samples) ]

Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 1.46501s
Throughput: 11.0299 M/s

 Performance counter stats for './orderbook_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

    10,919,167,966      task-clock                       #    0.999 CPUs utilized             
    43,161,504,405      cycles                           #    3.953 GHz                         (83.33%)
    96,960,338,212      instructions                     #    2.25  insn per cycle              (83.33%)
     1,014,259,823      cache-references                 #   92.888 M/sec                       (83.34%)
        77,639,181      cache-misses                     #    7.65% of all cache refs           (83.34%)
    22,148,937,695      branches                         #    2.028 G/sec                       (83.33%)
       237,172,224      branch-misses                    #    1.07% of all branches             (83.33%)

      10.925993988 seconds time elapsed

       9.346007000 seconds user
       1.574664000 seconds sys
```

---


## [Baseline] Initial Orderbook Implementation
**Date:** April 16, 2026
**Commit:** `6c6738e`

### Results
* **Throughput:** 5.27355 Million messages / second
* **Time to Process 16M messages:** 3.06415s
* **Bottlenecks Identified:** The Flame Graph shows on orderbook operations Cancel and Modify, significant time
  used to erase entries from the vector of orders. Likely need a different data structure for storing orders. Also on Add operations, significant CPU usage to insert into the vector. Need to possibly rethink these operations. Additionally, a huge number of cache misses. Need to identify a way to better utilize the cache for these operations. 

### Raw Perf Output
```text
Processed 16158945 events in 3.06415s
Throughput: 5.27355 M/s
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.209 MB /home/r/Desktop/Backtester/benchmarks/orderbook_perf_harness/perf.data (1561 samples) ]

Cached 16158945 events.
Benchmarking MarketStateManager::OnMarketEvent...
Processed 16158945 events in 3.08385s
Throughput: 5.23986 M/s

 Performance counter stats for './orderbook_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

    16,080,091,896      task-clock                       #    1.000 CPUs utilized             
    49,557,869,775      cycles                           #    3.082 GHz                         (83.33%)
   106,145,741,833      instructions                     #    2.14  insn per cycle              (83.33%)
     1,384,212,092      cache-references                 #   86.082 M/sec                       (83.33%)
       167,281,581      cache-misses                     #   12.08% of all cache refs           (83.33%)
    24,387,943,398      branches                         #    1.517 G/sec                       (83.33%)
       288,094,989      branch-misses                    #    1.18% of all branches             (83.33%)

      16.086082064 seconds time elapsed

      13.910550000 seconds user
       2.171305000 seconds sys
```

# Performance Optimization Log: Data Ingestion Pipeline (Thinkpad T1)

## Hardware & Environment
* **OS:** Linux (Ubuntu)
* **Compiler:** GCC (g++) with -O3 -fno-omit-frame-pointer
* **Data Source:** Databento MBO (16.158945 million messages, zstd compressed)

---

## Store data stream readers in vector for ~59% time reduction
**Date:** April 14, 2026
**Commit:** `58deecc`

### Results
* **Throughput:** 2.21269 Million messages / second
* **Time to Process 16M messages:** 7.30285s 
* **Bottlenecks Identified:** ReadLine now takes up ~33% of CPU cycles and is the largest single operation. _memchr still takes up ~21% of CPU cycles

### Raw Perf Output
```text
Processed 16158945 messages.
Time: 7.30285s (2.21269 M/s)
total_volume: 28907885
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.101 MB /home/r/Desktop/Backtester/benchmarks/reader_perf_data/perf.data (725 samples) ]

Processed 16158945 messages.
Time: 7.48861s (2.1578 M/s)
total_volume: 28907885

 Performance counter stats for './reader_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

     7,497,400,896      task-clock                       #    1.001 CPUs utilized             
    29,613,912,678      cycles                           #    3.950 GHz                         (83.33%)
    80,820,057,033      instructions                     #    2.73  insn per cycle              (83.32%)
       261,643,572      cache-references                 #   34.898 M/sec                       (83.34%)
        11,191,116      cache-misses                     #    4.28% of all cache refs           (83.34%)
    18,920,338,247      branches                         #    2.524 G/sec                       (83.34%)
       104,917,997      branch-misses                    #    0.55% of all branches             (83.33%)

       7.492516977 seconds time elapsed

       7.430790000 seconds user
       0.068071000 seconds sys
```

## [Baseline] Initial Data Ingestion Implementation
**Date:** April 11, 2026
**Commit:** `3a8a473`

### Results
* **Throughput:** 0.912276 Million messages / second
* **Time to Process 16M messages:** 17.7128s
* **Bottlenecks Identified:** The Flame Graph shows up front memory movement and allocation taking 33% of CPU cycles,`DataReaderManager::ParseMboLineToEvent` also consuming ~33% of CPU cycles

### Raw Perf Output
```text
Processed 16158945 messages.
Time: 17.7128s (0.912276 M/s)
total_volume: 28907885
[ perf record: Woken up 1 times to write data ]
[ perf record: Captured and wrote 0.208 MB ~/Backtester/benchmarks/reader_perf_data/perf.data (1756 samples) ]

Processed 16158945 messages.
Time: 18.8253s (0.858361 M/s)
total_volume: 28907885

 Performance counter stats for './reader_perf_harness ../test/test_data/ES-glbx-20251105.mbo.csv.zst':

    18,827,141,572      task-clock                       #    1.000 CPUs utilized             
    60,585,157,123      cycles                           #    3.218 GHz                         (83.33%)
   151,177,574,158      instructions                     #    2.50  insn per cycle              (83.34%)
       388,778,133      cache-references                 #   20.650 M/sec                       (83.33%)
        14,722,783      cache-misses                     #    3.79% of all cache refs           (83.34%)
    37,579,466,463      branches                         #    1.996 G/sec                       (83.33%)
       192,320,333      branch-misses                    #    0.51% of all branches             (83.34%)

      18.830887616 seconds time elapsed

      18.762527000 seconds user
       0.065991000 seconds sys
```

