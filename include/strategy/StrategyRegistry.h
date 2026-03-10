#pragma once
#include <string>
#include <unordered_map>
#include <functional>
#include <memory>
#include "IStrategy.h"

namespace backtester{

using StrategyFactoryFunc = std::function<std::unique_ptr<IStrategy>()>;

class StrategyRegistry {
public:
    static std::unique_ptr<IStrategy> Create(const std::string& name) {
        auto& map = GetRegistryMap();
        auto it = map.find(name);
        if (it != map.end()) {
            return it->second(); // Call the factory function
        }
        return nullptr;
    }

    static void Register(const std::string& name, StrategyFactoryFunc factory) {
        GetRegistryMap()[name] = factory;
    }

private:
    // 3. The Meyer's Singleton Trick
    // We put the map inside a static method rather than a static member variable.
    // This guarantees it is initialized the VERY FIRST time it is called,
    // avoiding crashes when macros run before main().
    static std::unordered_map<std::string, StrategyFactoryFunc>& GetRegistryMap() {
        static std::unordered_map<std::string, StrategyFactoryFunc> instance;
        return instance;
    }
};
// class StrategyRegistry {
// public:
//     static StrategyRegistry& GetInstance() {
//         static StrategyRegistry instance;
//         return instance;
//     }

//     void Register(const std::string& name, StrategyCreator creator) {
//         creators_[name] = creator;
//     }

//     std::unique_ptr<IStrategy> Create(const std::string& name) {
//         if (creators_.find(name) != creators_.end()) {
//             return creators_[name](); // Call the function to instantiate
//         }
//         return nullptr; // Or throw an exception
//     }

// private:
//     std::unordered_map<std::string, StrategyCreator> creators_;
//     StrategyRegistry() = default;
// };

// struct StrategyRegistrar {
//     StrategyRegistrar(const std::string& name, StrategyCreator creator) {
//         StrategyRegistry::GetInstance().Register(name, creator);
//     }
// };

// #define REGISTER_STRATEGY(ClassName, StringName) \
//     static StrategyRegistrar global_registrar_##ClassName( \
//         StringName, \
//         []() -> std::unique_ptr<IStrategy> { return std::make_unique<ClassName>(); } \
//     )
#define REGISTER_STRATEGY(ClassName, StringName) \
    namespace { \
        /* Create a dummy variable to force the registration to run before main() */ \
        static bool const registered_##ClassName = []() { \
            backtester::StrategyRegistry::Register(StringName, []() { \
                return std::make_unique<ClassName>(); \
            }); \
            return true; \
        }(); \
    }

}