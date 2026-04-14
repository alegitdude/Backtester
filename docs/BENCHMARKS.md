# Performance Optimization Log: Data Ingestion Pipeline

## Hardware & Environment
* **OS:** Linux (Ubuntu)
* **Compiler:** GCC (g++) with -O3 -fno-omit-frame-pointer
* **Data Source:** Databento MBO (16.158945 million messages, zstd compressed)

---

## [Baseline] Initial Implementation
**Date:** April 11, 2026
**Commit:** `3a8a473`

### Results
* **Throughput:** 0.912276 Million messages / second
* **Time to Process 16M messages:** 17.7128s
* **Bottlenecks Identified:** The Flame Graph shows up front memory movement and allocation taking 33% of CPU cylces,`DataReaderManager::ParseMboLineToEvent` also consuming ~33% of CPU cycles

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