# Sample backtest output
These reports and graph are real sample outputs generated from running the MovAvgCross
test strategy in src/strategy/MovAvgCross.cpp. The backtest is run using the sample configuration 
file found in ./config/demo.json, 

- Config: `config/demo.json` (defaults: 100k cash, MovAvgCrossMin 5/10)
- Data: ES futures MBO, full session 2025-11-05 (16.1M messages)

To produce your own report, run `./Backtester <path to config file>` 