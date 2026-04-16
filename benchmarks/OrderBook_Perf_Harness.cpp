#include "../include/data_ingestion/DataReaderManager.h"
#include "../include/market_state/MarketStateManager.h"
#include "../include/core/EventQueue.h"
#include "../include/core/DefaultConfig.h"
#include "../include/core/Types.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <algorithm>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: ./orderbook_perf_harness <path_to_mbo_file.zst>\n";
        return 1;
    }
    std::string filepath = argv[1];
    backtester::AppConfig config = backtester::GetDefaultConfig();
   
    backtester::EventQueue event_queue;
    backtester::DataReaderManager reader;

    std::vector<uint32_t> traded_instr_ids = {294973};
    backtester::MarketStateManager market_state;
    market_state.Initialize(config.active_instruments, traded_instr_ids);

    if (!reader.RegisterAndInitStreams(config.data_configs)) {
        std::cerr << "Failed to init reader.\n";
        return 1;
    }

    std::cout << "Preloading events into memory...\n";
    std::vector<MarketByOrderEvent> event_cache;
    event_cache.reserve(16500000); 

    while (auto event_ptr = reader.LoadNextEventFromSource("ES")) {
        event_cache.push_back(*event_ptr);
    }
    std::cout << "Cached " << event_cache.size() << " events.\n";

    // 3. Phase 2: The Blast (Timed)
    std::cout << "Benchmarking MarketStateManager::OnMarketEvent...\n";
    auto start = std::chrono::high_resolution_clock::now();

    for (const auto& event : event_cache) {
        market_state.OnMarketEvent(event);
    }

    auto end = std::chrono::high_resolution_clock::now();
    
    // 4. Results
    std::chrono::duration<double> elapsed = end - start;
    double m_msgs = event_cache.size() / 1000000.0;
    std::cout << "Processed " << event_cache.size() << " events in " << elapsed.count() << "s\n";
    std::cout << "Throughput: " << (m_msgs / elapsed.count()) << " M/s\n";

    return 0;
}