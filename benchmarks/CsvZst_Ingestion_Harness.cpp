#include "../include/data_ingestion/DataReaderManager.h"
#include "../include/core/ConfigParser.h"
#include <iostream>
#include <chrono>

int main(int argc, char** argv) {
    std::string config_path = (argc > 1) ? argv[1] : BENCH_CONFIG_DEFAULT;

    size_t message_count = 0;
    long long total_volume = 0;

    backtester::AppConfig config = backtester::ParseConfigToObj(config_path);
    auto start = std::chrono::high_resolution_clock::now();
    ////////////////////////////////////////////////////////////////////////////

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