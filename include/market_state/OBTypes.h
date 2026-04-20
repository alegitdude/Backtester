#pragma once
#include <cstdint>

namespace backtester {

struct LevelQueue {
    uint32_t size{0};
    uint32_t count{0};
};

}