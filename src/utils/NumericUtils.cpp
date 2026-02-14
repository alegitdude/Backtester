#include <iostream>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace backtester {

namespace numericUtils{
    
long long doubleToFixedPoint(double val) {
    const double scale = 1'000'000'000.0;
    
    // 1. Check for NaN
    if (std::isnan(val)) throw std::invalid_argument("NaN cannot be converted");

    // 2. Check for Overflow/Underflow
    // Max double allowed is approx LLONG_MAX / scale
    if (val > (static_cast<double>(std::numeric_limits<long long>::max()) / scale) ||
        val < (static_cast<double>(std::numeric_limits<long long>::min()) / scale)) {
        throw std::out_of_range("Double value too large for scaled long long");
    }

    // 3. Scale and Round
    return std::llround(val * scale);
}

}
}