# Sample MovAvgCross backtest output
These reports and graph are real sample outputs generated from running the MovAvgCross
test strategy in src/strategy/user_strategies/MovAvgCross.cpp. The backtest is run using the sample configuration file found in ./config/demo.json and runs through a full cash session (9:30am - 3pm EST) 

- Config: `config/demo.json` (defaults: 100k cash, MovAvgCrossMin 5/10)
- Data: ES futures MBO, full session 2025-11-05 (16.1M messages)

To produce your own report, run `./Backtester ./config/demo.json` from project root. 