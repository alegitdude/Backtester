#include "../include/data_ingestion/DataReaderManager.h"
#include "../include/core/DefaultConfig.h"
#include <iostream>
#include <chrono>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_mbo_file.zst>" << std::endl;
        return 1;
    }

    size_t message_count = 0;
    long long total_volume = 0;

    auto start = std::chrono::high_resolution_clock::now();
    ////////////////////////////////////////////////////////////////////////////

    backtester::AppConfig config = backtester::GetDefaultConfig();
    config.data_configs[0].data_filepath = argv[1];
    backtester::DataReaderManager data_reader_manager;
    data_reader_manager.RegisterAndInitStreams(config.data_configs);
    std::string source = config.data_configs[0].data_source_name;

    while (true) {
        auto event_ptr = data_reader_manager.LoadNextEventFromSource(source);
        if (event_ptr) {
            total_volume += event_ptr->size;
            message_count++;
        } else{
            break;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> diff = end - start;

    std::cout << "Processed " << message_count << " messages." << std::endl;
    std::cout << "Time: " << diff.count() << "s (" << (message_count / diff.count() / 1e6) << " M/s)" << std::endl;
    std::cout << "total_volume: " << total_volume << std::endl;

    return 0;
}