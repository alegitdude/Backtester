#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "IStrategy.h"

namespace backtester{
// The global map that holds "Strategy Name" -> "Function that creates the Strategy"
using StrategyCreator = std::function<std::unique_ptr<IStrategy>()>;

class StrategyRegistry {
public:
    static StrategyRegistry& GetInstance() {
        static StrategyRegistry instance;
        return instance;
    }

    void Register(const std::string& name, StrategyCreator creator) {
        creators_[name] = creator;
    }

    std::unique_ptr<IStrategy> Create(const std::string& name) {
        if (creators_.find(name) != creators_.end()) {
            return creators_[name](); // Call the function to instantiate
        }
        return nullptr; // Or throw an exception
    }

private:
    std::unordered_map<std::string, StrategyCreator> creators_;
    StrategyRegistry() = default;
};

struct StrategyRegistrar {
    StrategyRegistrar(const std::string& name, StrategyCreator creator) {
        StrategyRegistry::GetInstance().Register(name, creator);
    }
};

#define REGISTER_STRATEGY(ClassName, StringName) \
    static StrategyRegistrar global_registrar_##ClassName( \
        StringName, \
        []() -> std::unique_ptr<IStrategy> { return std::make_unique<ClassName>(); } \
    )

}