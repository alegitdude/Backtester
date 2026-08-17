# Sample SimpleMarketMaker strategy backtest output
These reports and graph are real sample outputs generated from running the SimpleMarketMaker test strategy in src/strategy/user_strategies/SimpleMarketMaker.cpp. The backtest is run using the sample configuration 
file found in ./config/demo-mm-sample.json, 

- Config: `config/demo-mm-sample.json` (defaults: 100k cash, SimpleMarketMaker, 2,1,2,1,1,0 )
- Data: ES futures MBO, overnight session 2025-11-04 (500k messages)

To produce your own report, run `./Backtester ./config/demo-mm-sample.json` from project root. 