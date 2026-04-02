#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "IStrategy.h"

namespace backtester{

using StrategyFactoryFunc = std::function<std::unique_ptr<IStrategy>(const std::string, const IMarketDataProvider&)>;

class StrategyRegistry {
public:
    static std::unique_ptr<IStrategy> Create(const std::string& name,
    const IMarketDataProvider& market_data) {
        auto& map = GetRegistryMap();
        auto it = map.find(name);
        if (it != map.end()) {
            return it->second(name, market_data); // Call the factory function
        }
        return nullptr;
    }

    static void Register(const std::string& name, StrategyFactoryFunc factory) {
        GetRegistryMap()[name] = factory;
    }

private:
    static std::unordered_map<std::string, StrategyFactoryFunc>& GetRegistryMap() {
        static std::unordered_map<std::string, StrategyFactoryFunc> instance;
        return instance;
    }
};

// #define REGISTER_STRATEGY(ClassName, StringName) \
//     static StrategyRegistrar global_registrar_##ClassName( \
//         StringName, \
//         []() -> std::unique_ptr<IStrategy> { return std::make_unique<ClassName>(); } \
//     )
#define REGISTER_STRATEGY(ClassName, StringName) \
    namespace { \
        static bool const registered_##ClassName = []() { \
            backtester::StrategyRegistry::Register(StringName, \
                [](const std::string strategy_id, const backtester::IMarketDataProvider& md) { \
                    return std::make_unique<ClassName>(strategy_id, md); \
                }); \
            return true; \
        }(); \
    }

}